// #6506: a session tab whose file is gone stays as a placeholder (#5585)
// unless KeepMissingSessionTabs = false, and Close Tabs of Missing Files
// closes such tabs in one go.
//
// Run: bun tests/issue-6506.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function tabState(path: string): string {
  return `      [
        FilePath = ${path}
        DisplayMode = automatic
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]`;
}

function seedSettings(dir: string, present: string, missing: string, keep: boolean): void {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
ReuseInstance = false
UseTabs = true
LazyLoading = false
KeepMissingSessionTabs = ${keep}
SessionData [
  [
    TabStates [
${tabState(present)}
${tabState(missing)}
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`,
  );
}

async function tabTexts(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestTabWidths, []))[1] ?? "");
}

// waits until the tab strip does (want) or doesn't (!want) show the missing file
async function waitMissingTab(client: ControlClient, want: boolean, what: string): Promise<void> {
  const deadline = Date.now() + 10000;
  let tabs = "";
  for (;;) {
    tabs = await tabTexts(client);
    if (tabs.includes("gone") === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6506: ${what}; tabs:\n${tabs}`);
    }
    await sleep(100);
  }
}

async function runSession(dir: string, keep: boolean): Promise<void> {
  const present = join(dir, "here.pdf");
  writeFileSync(present, makePdf(), "latin1");
  const missing = join(dir, "gone.pdf");
  rmSync(missing, { force: true });
  seedSettings(dir, present, missing, keep);

  const log = join(dir, "log.txt");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, "-log-to-file", log], {
    saveSettings: true,
  });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);
    if (!(await tabTexts(client)).includes("here")) {
      throw new Error(`issue-6506: the present file's tab is missing:\n${await tabTexts(client)}`);
    }
    if (!keep) {
      await waitMissingTab(client, false, "KeepMissingSessionTabs = false still restored the missing file's tab");
      return;
    }
    await waitMissingTab(client, true, "the missing file's placeholder tab was not restored");
    if (USE_NG) {
      await client.request(ControlCommand.TestInvokeCommand, ["CmdCloseMissingTabs"]);
    } else {
      sendCommand(frame, cmdId("CmdCloseMissingTabs"));
    }
    await waitMissingTab(client, false, "Close Tabs of Missing Files left the placeholder tab");
    if (!(await tabTexts(client)).includes("here")) {
      throw new Error("issue-6506: Close Tabs of Missing Files closed the present file's tab");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  for (const keep of [true, false]) {
    const dir = tmpPath(`issue-6506-${keep}`);
    rmSync(dir, { recursive: true, force: true });
    mkdirSync(dir, { recursive: true });
    await runSession(dir, keep);
  }
  console.log("issue-6506: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
