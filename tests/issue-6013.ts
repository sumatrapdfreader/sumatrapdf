// #6013: Help: Manual opens next to the main window (upper half, side with more
// room), remembers that position within a session, and stays fully on-screen
// if the saved rect is off-screen (resolution / monitor change).
//
// Needs WebView2. If the documentation window never appears, skip.
//
// Runs on its own empty -appdata: the first check is about where the window
// goes when nothing is remembered yet, and HelpWindowPos in the settings the
// exe under test happens to load (a portable SumatraPDF-settings.txt next to
// out/<build>/SumatraPDF.exe, say) would short-circuit that placement.
//
// Run: bun tests/issue-6013.ts [--no-build]

import { mkdirSync, rmSync } from "node:fs";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowRect,
  getWindowText,
  getWorkArea,
  isWindowVisible,
  postMessage,
  setWindowPos,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const HELP_TITLE = "SumatraPDF Documentation";

function findHelpWindow(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid) {
      return true;
    }
    if (getWindowText(hwnd) === HELP_TITLE) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitHelpShown(pid: number, timeoutMs: number): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const hwnd = findHelpWindow(pid);
    if (hwnd && isWindowVisible(hwnd)) {
      return hwnd;
    }
    if (Date.now() > deadline) {
      return 0;
    }
    await sleep(50);
  }
}

async function waitHelpGone(pid: number, timeoutMs: number): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const hwnd = findHelpWindow(pid);
    if (!hwnd || !isWindowVisible(hwnd)) {
      return true;
    }
    if (Date.now() > deadline) {
      return false;
    }
    await sleep(50);
  }
}

function rectSize(r: { left: number; top: number; right: number; bottom: number }) {
  return { x: r.left, y: r.top, dx: r.right - r.left, dy: r.bottom - r.top };
}

function fullyOnWorkArea(
  r: { left: number; top: number; right: number; bottom: number },
  wa: { left: number; top: number; right: number; bottom: number },
  slack = 8,
): boolean {
  return (
    r.left >= wa.left - slack && r.top >= wa.top - slack && r.right <= wa.right + slack && r.bottom <= wa.bottom + slack
  );
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6013-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  const pid = proc.pid!;
  try {
    sendCommandSync(frame, cmdId("CmdHelpOpenManual"));
    const help = await waitHelpShown(pid, 15000);
    if (!help) {
      console.log("SKIP issue-6013: Help: Manual window did not open (WebView2 missing?)");
      return;
    }

    const wa = getWorkArea();
    const frameR = getWindowRect(frame);
    const helpR = getWindowRect(help);
    if (!fullyOnWorkArea(helpR, wa)) {
      throw new Error(
        `issue-6013: help window ${JSON.stringify(helpR)} is not fully on the work area ${JSON.stringify(wa)}`,
      );
    }

    const frameMidY = frameR.top + (frameR.bottom - frameR.top) / 2;
    if (helpR.top > frameMidY + 8) {
      throw new Error(
        `issue-6013: help window top ${helpR.top} is not in the upper half of the main window (mid ${frameMidY})`,
      );
    }

    const leftSpace = frameR.left - wa.left;
    const rightSpace = wa.right - frameR.right;
    if (leftSpace > rightSpace + 20) {
      if (helpR.left > frameR.left + 20) {
        throw new Error(
          `issue-6013: more space on the left (${leftSpace} vs ${rightSpace}) but help left ${helpR.left} is not on that side of the frame ${frameR.left}`,
        );
      }
    } else if (rightSpace > leftSpace + 20) {
      if (helpR.right < frameR.right - 20) {
        throw new Error(
          `issue-6013: more space on the right (${rightSpace} vs ${leftSpace}) but help right ${helpR.right} is not on that side of the frame ${frameR.right}`,
        );
      }
    }

    const moved = {
      x: wa.left + 40,
      y: wa.top + 50,
      dx: Math.max(480, helpR.right - helpR.left),
      dy: Math.max(400, helpR.bottom - helpR.top),
    };
    setWindowPos(help, moved.x, moved.y, moved.dx, moved.dy);
    postMessage(help, WM_CLOSE, 0, 0);
    if (!(await waitHelpGone(pid, 8000))) {
      throw new Error("issue-6013: help window did not close");
    }

    sendCommandSync(frame, cmdId("CmdHelpOpenManual"));
    const help2 = await waitHelpShown(pid, 15000);
    if (!help2) {
      throw new Error("issue-6013: help window did not reopen");
    }
    const restored = rectSize(getWindowRect(help2));
    const slack = 16;
    if (
      Math.abs(restored.x - moved.x) > slack ||
      Math.abs(restored.y - moved.y) > slack ||
      Math.abs(restored.dx - moved.dx) > slack ||
      Math.abs(restored.dy - moved.dy) > slack
    ) {
      throw new Error(
        `issue-6013: help window did not restore saved position: got ${JSON.stringify(restored)} want ${JSON.stringify(moved)}`,
      );
    }

    // off-screen saved rect (as after a resolution change) must come back fully visible
    setWindowPos(help2, 20000, 20000, restored.dx, restored.dy);
    postMessage(help2, WM_CLOSE, 0, 0);
    if (!(await waitHelpGone(pid, 8000))) {
      throw new Error("issue-6013: help window did not close after off-screen move");
    }
    sendCommandSync(frame, cmdId("CmdHelpOpenManual"));
    const help3 = await waitHelpShown(pid, 15000);
    if (!help3) {
      throw new Error("issue-6013: help window did not reopen after off-screen move");
    }
    const clamped = getWindowRect(help3);
    if (!fullyOnWorkArea(clamped, wa)) {
      throw new Error(
        `issue-6013: off-screen saved position was not clamped on-screen: ${JSON.stringify(clamped)} work ${JSON.stringify(wa)}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
