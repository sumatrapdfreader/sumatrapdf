// Restoring a session tab whose file exists but fails to load (no lazy
// loading) must leave one tab for it. ng's load showed its error tab and the
// restore then added a placeholder tab for the same file.
//
// Run: bun tests/session-restore-broken-file.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

function seedSettings(dir: string, pdf: string): void {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
LazyLoading = false
ReuseInstance = false
UseTabs = true
ShowStartPage = false
SessionData [
  [
    TabStates [
      [
        FilePath = ${pdf}
        DisplayMode = continuous
        PageNo = 1
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

async function tabCount(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  const m = /\btabs=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`session-restore-broken-file: no tab count in UI state: ${s}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP session-restore-broken-file: ng only (orig has no tab count in its UI state)");
    return;
  }
  const dir = tmpPath("session-restore-broken-file");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "broken.pdf");
  writeFileSync(pdf, "%PDF-1.7\nthis is not a pdf\n", "latin1");
  seedSettings(dir, pdf);

  const { proc, client } = await launchControlled(["-appdata", dir], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await sleep(300);
    // the home tab and one for broken.pdf
    const n = await tabCount(client);
    if (n !== 2) {
      throw new Error(`session-restore-broken-file: ${n} tabs, want 2 (home + broken.pdf)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("session-restore-broken-file: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
