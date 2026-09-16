// Use-after-free of a lazy tab's TabState (crash 2026-09-16-01-15-62d8).
//
// A lazily restored tab borrows its TabState from gInitialSessionData. With
// RememberOpenedFiles = false, RememberSessionState() frees gSettings->sessionData
// and returns early, so SyncInitialSessionData() frees the snapshot and clones
// nothing: RefreshLazyTabStatePointers() has nothing to repoint to and every lazy
// tab keeps a freed pointer. Selecting that tab loads it and SetTabState() reads
// the freed TabState (crash in ParseStoredPagePos).
//
// Uses a scratch -appdata dir and no -for-testing, so the session is restored and
// ScheduleSaveSettings() is not suppressed.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommandSync } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const LAZY_PDF = resolve("tests/issue-1189.pdf");
const LOADED_PDF = resolve("tests/issue-6095.epub");

const APPDATA = tmpPath("lazy-tab-state-after-save-appdata");

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
  // RememberOpenedFiles = false is what makes the save drop the whole session
  const seed = `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
LazyLoading = true
UseTabs = true
RememberOpenedFiles = false
RememberStatePerDocument = true
ReuseInstance = false
SessionData [
  [
    TabStates [
${tabState(LAZY_PDF)}
${tabState(LOADED_PDF)}
    ]
    TabIndex = 2
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`;
  writeFileSync(join(APPDATA, "SumatraPDF-settings.txt"), seed, "utf8");
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  seedSettings();
  const { proc, client, frame } = await launchControlled(["-appdata", APPDATA], { saveSettings: true });
  try {
    await client.waitForRenderIdle(30000);

    // frees gInitialSessionData; the still-lazy first tab borrows from it
    sendCommandSync(frame, cmdId("CmdToggleFavoritesSort"));
    await sleep(500 * SLOW_BUILD_FACTOR);
    await client.waitForRenderIdle(30000);

    // selecting the lazy tab loads it and reads its TabState
    sendCommandSync(frame, cmdId("CmdPrevTab"));
    await sleep(500 * SLOW_BUILD_FACTOR);
    await client.waitForRenderIdle(30000);

    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await proc.exited;
    if (exitCode !== 0) {
      throw new Error(`lazy-tab-state-after-save: exit code ${exitCode}, want 0`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
