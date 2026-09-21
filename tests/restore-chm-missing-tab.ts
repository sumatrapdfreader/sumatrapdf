// Debug report `!gPluginMode` in MainWindow::IsDocLoaded (crash 2026-09-21-04-56-c4f8).
//
// Session restore loads a CHM (browser view) synchronously, so it becomes
// win->ctrl, while AddTabToWindow selects the last restored tab: a placeholder
// for a missing file. TabsSelect then finds the wanted tab already selected and
// returns without switching, so the first WM_MOVE of ShowMainWindow sees
// win->ctrl set and CurrentTab() without a document.
//
// Uses a scratch -appdata dir and no -for-testing, so the session is restored.
// Without -for-testing a debug report does not end the process, it only writes
// the report to stderr, so the test checks stderr.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";

import { killAndWait, killProcessesNamed, launchControlled, takeStderr } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const CHM = resolve("tests/issue-2737.chm");
const MISSING_EPUB = resolve("tests/tmp/restore-chm-missing-tab-does-not-exist.epub");

const APPDATA = tmpPath("restore-chm-missing-tab-appdata");

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
LazyLoading = false
UseTabs = true
RememberOpenedFiles = true
RememberStatePerDocument = true
ReuseInstance = false
SessionData [
  [
    TabStates [
${tabState(CHM)}
${tabState(MISSING_EPUB)}
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
  const { proc, client } = await launchControlled(["-appdata", APPDATA], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    // the current tab is the placeholder, so there is no render to wait on
    await sleep(500 * SLOW_BUILD_FACTOR);
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await proc.exited;
    const stderr = await takeStderr(proc);
    if (stderr.includes("MainWindow.cpp") || stderr.includes("IsDocLoaded")) {
      throw new Error(`restore-chm-missing-tab: debug report fired:\n${stderr}`);
    }
    if (exitCode !== 0) {
      throw new Error(`restore-chm-missing-tab: exit code ${exitCode}, want 0`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
