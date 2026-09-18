// Use-after-free of a closed tab's TOC tree (crash 2026-09-18-00-01-6f48).
//
// After session restore with the last tab selected, TabsSelect() is a no-op and
// currentTabTemp stays null. RemoveTab() asked CurrentTab() whether the closed
// tab was current only after taking it out of the tab strip, so the answer was
// "no": win->ctrl and the TOC tree view kept pointing at the closed tab's
// document, which was then freed. The next TOC paint read the freed tree.
//
// Uses a scratch -appdata dir and no -for-testing, so the session is restored.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommand, takeStderr } from "./win-automation.ts";
import { getWindowText, sleep } from "./winapi.ts";

const OTHER_PDF = resolve("tests/issue-1809.pdf");
// has an outline, so the TOC tree view shows its items
const TOC_PDF = resolve("tests/issue-6101.pdf");

const APPDATA = tmpPath("restored-tab-close-toc-appdata");

function seedSettings(): void {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
  const tabState = (path: string, showToc: boolean) => `      [
        FilePath = ${path}
        DisplayMode = single page
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = ${showToc}
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
${tabState(OTHER_PDF, false)}
${tabState(TOC_PDF, true)}
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
  let title = "";
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);

    // close the restored, current tab; the other tab must become current.
    // Posted: SendMessage would deadlock with the app writing a report to our stderr
    sendCommand(frame, cmdId("CmdClose"));
    await sleep(500 * SLOW_BUILD_FACTOR);
    await client.waitForRenderIdle(30000);
    title = getWindowText(frame);

    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await proc.exited;
    const stderr = await takeStderr(proc);
    if (stderr) {
      throw new Error(`restored-tab-close-toc: report on stderr:\n${stderr}`);
    }
    if (exitCode !== 0) {
      throw new Error(`restored-tab-close-toc: exit code ${exitCode}, want 0`);
    }
    if (!title.includes("issue-1809.pdf")) {
      throw new Error(`restored-tab-close-toc: title '${title}', want the remaining tab issue-1809.pdf`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
