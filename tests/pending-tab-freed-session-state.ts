// Use-after-free in SetTabState (crashes 2026-09-22-11-33-eabd and nine more).
//
// Session restore without lazy loading starts every tab's load at once. A tab
// that is not current when its load finishes is parked as LoadedPending with
// its LoadArgs, whose tabState points into gInitialSessionData. Closing the
// current tab saves settings, which rebuilds that snapshot and frees the old
// TabState objects, then selects the pending tab: LoadDocumentFinish restores
// its view from the freed TabState.
//
// Only an ASan build detects the freed read reliably (the debug build uses the
// release CRT, so freed memory keeps its contents). Uses a scratch -appdata dir
// and no -for-testing so the session is restored.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommand, takeStderr } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const FIRST_PDF = resolve("tests/issue-1189.pdf");
const SECOND_PDF = resolve("tests/issue-3219.pdf");
const PENDING_PDF = resolve("tests/issue-4157.pdf");

const APPDATA = tmpPath("pending-tab-freed-session-state-appdata");

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
${tabState(FIRST_PDF)}
${tabState(SECOND_PDF)}
${tabState(PENDING_PDF)}
    ]
    TabIndex = 1
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
    // not waitForSessionRestored: it reports "loading" as long as a tab is
    // parked as LoadedPending, which is exactly the state this test needs
    await client.waitForRenderIdle(60000);
    // let the second tab's load finish and park as LoadedPending
    await sleep(1000 * SLOW_BUILD_FACTOR);

    // the first close attaches the second tab and then saves the session,
    // freeing the snapshot the third tab's parked LoadArgs still point into;
    // the second close selects that third tab. Posted: SendMessage would
    // deadlock with the app writing a report to our stderr
    for (let i = 0; i < 2; i++) {
      sendCommand(frame, cmdId("CmdClose"));
      await sleep(500 * SLOW_BUILD_FACTOR);
      await client.waitForRenderIdle(30000);
    }

    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await proc.exited;
    const stderr = await takeStderr(proc);
    if (stderr.includes("AddressSanitizer") || stderr.includes("SetTabState")) {
      throw new Error(`pending-tab-freed-session-state: freed TabState read:\n${stderr}`);
    }
    if (exitCode !== 0) {
      throw new Error(`pending-tab-freed-session-state: exit code ${exitCode}, want 0`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
