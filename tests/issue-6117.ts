// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6117
//
// With the focus on the Find window's results list, PageUp / PageDown scrolled
// the document instead of paging the list: they accelerated to CmdScrollUpPage
// / CmdScrollDownPage before the list ever saw them. Home / End were already
// excluded from the accelerator tables a focused control uses, so they worked.
//
// Run: bun tests/issue-6117.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import {
  enumWindows,
  getClassName,
  getFocusedHwnd,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  postMessage,
  sendText,
  sleep,
  WM_KEYDOWN,
} from "./winapi";
import { clickAt, killAndWait, launchControlled, sendCommand } from "./win-automation";

const VK_PRIOR = 0x21;
const VK_NEXT = 0x22;
const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const TERM = "the";

// The Find window has no class of its own; it's an owned popup with the
// default window class, so pick it out by title.
function findFindWindow(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    const r = getWindowRect(hwnd);
    if (getClassName(hwnd) === "SumatraWgDefaultWinClass" && getWindowText(hwnd) === "Find" && r.bottom - r.top > 120) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// "sel" is the results list's current item. NOTREADY until the count scan ends.
async function resultsSel(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 15000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestFindResultsOrder, [TERM, 1]);
    raw = String(res[1] ?? "");
    const m = /sel=(-?\d+)/.exec(raw);
    if (m) {
      return +m[1]!;
    }
    await sleep(200);
  }
  throw new Error(`issue-6117: the result scan never finished\n${raw}`);
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-6117");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(appData, "doc.pdf");
  copyFileSync(SRC_PDF, pdf);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "SearchUIFloating = true", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(
    ["-appdata", appData, "-view", "continuous", "-zoom", "120", pdf],
    { saveSettings: true },
  );
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const pid = getWindowPid(frame);

    sendCommand(frame, cmdId("CmdFindFirst"));
    const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
    let findWnd = 0;
    while (Date.now() < deadline) {
      findWnd = findFindWindow(pid);
      if (findWnd) {
        break;
      }
      await sleep(100);
    }
    if (!findWnd) {
      throw new Error("issue-6117: the Find window did not open");
    }
    sendText(getFocusedHwnd(frame), TERM, true);
    await resultsSel(client); // wait out the count scan

    // focus the results list by clicking a row, the way the report describes
    await clickAt(findWnd, 60, 120, 500 * SLOW_BUILD_FACTOR);
    const start = await resultsSel(client);
    if (start < 0) {
      throw new Error("issue-6117: clicking a result did not select it");
    }

    // PageDown must move the list by more than one row and must not be eaten
    // by the document's scroll accelerator
    // one read per key: TestFindResultsOrder re-arms the scan when it is not
    // ready, so polling it in a loop disturbs what we are measuring
    postMessage(findWnd, WM_KEYDOWN, VK_NEXT, 0);
    await sleep(800 * SLOW_BUILD_FACTOR);
    const afterDown = await resultsSel(client);
    if (afterDown <= start + 1) {
      throw new Error(`issue-6117: PageDown moved the selection ${start} -> ${afterDown}, expected a whole page`);
    }

    postMessage(findWnd, WM_KEYDOWN, VK_PRIOR, 0);
    await sleep(800 * SLOW_BUILD_FACTOR);
    const afterUp = await resultsSel(client);
    if (afterUp >= afterDown) {
      throw new Error(`issue-6117: PageUp moved the selection ${afterDown} -> ${afterUp}, expected it to go back`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
