// Debug report `!gPluginMode` in MainWindow::IsDocLoaded (crash 2026-09-22-12-30-a066).
//
// Closing the current tab while an annotation placement is active: RemoveTab
// has already nulled win->ctrl and selected the neighbouring (loaded) tab when
// LoadModelIntoTab cancels the placement, and the toolbar update it triggers
// sees CurrentTab()->ctrl set while win->ctrl is null.
//
// Uses a scratch -appdata dir and no -for-testing, so the session is restored.
// Without -for-testing a debug report does not end the process, it only writes
// the report to stderr, so the test checks stderr.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, takeStderr } from "./win-automation.ts";

const FIRST_PDF = resolve("tests/issue-1189.pdf");
const SECOND_PDF = resolve("tests/issue-3219.pdf");

const APPDATA = tmpPath("close-tab-during-placement-appdata");

function seedSettings(): void {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
  const tabState = (path: string) => `      [
        FilePath = ${path}
        DisplayMode = single page
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]`;
  const seed = `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
LazyLoading = true
UseTabs = true
RememberOpenedFiles = true
RememberStatePerDocument = true
ReuseInstance = false
SessionData [
  [
    TabStates [
${tabState(FIRST_PDF)}
${tabState(SECOND_PDF)}
    ]
    TabIndex = 2
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`;
  writeFileSync(join(APPDATA, "SumatraPDF-settings.txt"), seed, "utf8");
}

async function currentPath(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const raw = String(res[1] ?? "").trim();
  const m = /^path=(.+?) page=/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`close-tab-during-placement: could not read current tab: ${raw}`);
  }
  return resolve(m[1]!);
}

async function placementActive(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`close-tab-during-placement: could not read placement state: ${raw.trim()}`);
  }
  return /Placement active=1/.test(raw);
}

async function invokeCommand(client: ControlClient, command: string): Promise<void> {
  const [code, detail] = await client.request(ControlCommand.TestInvokeCommand, [command]);
  if (code !== 0) {
    throw new Error(`close-tab-during-placement: ${command} failed: ${detail}`);
  }
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  seedSettings();
  const { proc, client } = await launchControlled(["-appdata", APPDATA], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);

    // Resolve the command target after session restore, when the document window is ready.
    await invokeCommand(client, "CmdPrevTab");
    await pollUntil(
      () => currentPath(client),
      (path) => path === FIRST_PDF,
      {
        timeoutMs: 5000 * SLOW_BUILD_FACTOR,
        error: (path) => `close-tab-during-placement: selected ${path}, want ${FIRST_PDF}`,
      },
    );
    await client.waitForRenderIdle(30000);

    await invokeCommand(client, "CmdCreateAnnotSquare");
    await pollUntil(
      () => placementActive(client),
      (active) => active,
      {
        timeoutMs: 5000 * SLOW_BUILD_FACTOR,
        error: "close-tab-during-placement: square placement did not start",
      },
    );

    await invokeCommand(client, "CmdClose");
    await pollUntil(
      () => currentPath(client),
      (path) => path === SECOND_PDF,
      {
        timeoutMs: 5000 * SLOW_BUILD_FACTOR,
        error: (path) => `close-tab-during-placement: selected ${path}, want ${SECOND_PDF}`,
      },
    );
    await client.waitForRenderIdle(30000);

    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await proc.exited;
    const stderr = await takeStderr(proc);
    if (stderr.includes("MainWindow.cpp") || stderr.includes("IsDocLoaded")) {
      throw new Error(`close-tab-during-placement: debug report fired:\n${stderr}`);
    }
    if (exitCode !== 0) {
      throw new Error(`close-tab-during-placement: exit code ${exitCode}, want 0`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
