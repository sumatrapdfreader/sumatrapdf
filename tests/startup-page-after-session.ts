// -page and -zoom apply to the file given on the command line even when a
// session was restored first (they used to apply only when no window existed).
//
// Run: bun tests/startup-page-after-session.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const name = "startup-page-after-session";
const kPages = 10;
const kPage = 7;

function makePdf(): string {
  const objects = ["<< /Type /Catalog /Pages 2 0 R >>", ""];
  const kids: number[] = [];
  for (let i = 0; i < kPages; i++) {
    objects.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
    kids.push(objects.length);
  }
  objects[1] = `<< /Type /Pages /Kids [${kids.map((n) => `${n} 0 R`).join(" ")}] /Count ${kPages} >>`;
  return assemblePdf(objects);
}

function seedSettings(dir: string, sessionFile: string): void {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
ReuseInstance = false
UseTabs = true
LazyLoading = false
SessionData [
  [
    TabStates [
      [
        FilePath = ${sessionFile}
        DisplayMode = automatic
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`,
  );
}

async function currentTab(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return String(res[1] ?? "").trim();
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const inSession = join(dir, "session.pdf");
  const onCmdLine = join(dir, "cmdline.pdf");
  writeFileSync(inSession, makePdf(), "latin1");
  writeFileSync(onCmdLine, makePdf(), "latin1");
  seedSettings(dir, inSession);

  const args = ["-appdata", dir, "-page", String(kPage), onCmdLine];
  const { proc, client } = await launchControlled(args, { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);
    const want = `path=${onCmdLine} page=${kPage}`;
    await pollUntil(
      () => currentTab(client),
      (s) => s.startsWith(want),
      { error: (s) => `${name}: want '${want}', got '${s}'` },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
