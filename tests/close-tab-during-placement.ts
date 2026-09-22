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
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommand, takeStderr } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

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

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  seedSettings();
  const { proc, client, frame } = await launchControlled(["-appdata", APPDATA], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);

    // load the lazy tab too, so the tab left after the close has a document
    // posted: SendMessage would deadlock with the app writing a report to our stderr
    sendCommand(frame, cmdId("CmdPrevTab"));
    await sleep(500 * SLOW_BUILD_FACTOR);
    await client.waitForRenderIdle(30000);

    sendCommand(frame, cmdId("CmdCreateAnnotSquare"));
    await sleep(300 * SLOW_BUILD_FACTOR);

    sendCommand(frame, cmdId("CmdClose"));
    await sleep(500 * SLOW_BUILD_FACTOR);
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
