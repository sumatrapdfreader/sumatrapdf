// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5958
//
// CmdToggleKeyboardHelp ("?") created the help window but nothing showed up on
// screen: the window is an owned WS_POPUP, GetParent() returns its owner, so
// WindowBase::SetVisibility() took the child-window path and only flipped the
// WS_VISIBLE style bit instead of calling ShowWindow(). The window existed and
// held the focus (the menu bar stopped responding) but was never composited.
//
// PrintWindow renders a never-shown window just fine, so a window capture
// proves nothing here. Read the pixels straight off the *screen* where the help
// window sits and require they changed when it opened and reverted when it
// closed.
//
// Run:  bun tests/issue-5958.ts [--no-build]

import { cmdId, runStandalone } from "./util.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  readWindowDCRow,
  sleep,
  type Rect,
} from "./winapi.ts";
import { launchSumatra, sendCommand, waitForFrame, killAndWait } from "./win-automation.ts";

const HELP_TITLE = "Keyboard Shortcuts";

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

async function waitForHelpWindow(pid: number, want: boolean, timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let hwnd = 0;
  while (Date.now() < deadline) {
    hwnd = findHelpWindow(pid);
    if (want === (hwnd !== 0 && isWindowVisible(hwnd))) {
      return hwnd;
    }
    await sleep(100);
  }
  return hwnd;
}

// hwnd 0 is the screen DC, so this is what a user would actually see
function screenPixels(r: Rect, inset: number): number[] {
  const y = Math.floor((r.top + r.bottom) / 2);
  return readWindowDCRow(0, r.left + inset, y, 24);
}

function sameRow(a: number[], b: number[]): boolean {
  return a.length === b.length && a.every((c, i) => c === b[i]);
}

function fmtRow(a: number[]): string {
  return a.map((c) => c.toString(16).padStart(6, "0")).join(" ");
}

export async function testit(): Promise<void> {
  const proc = launchSumatra([]);
  const pid = proc.pid!;
  try {
    const frame = await waitForFrame(pid);
    if (!frame) {
      throw new Error("issue-5958: main window did not appear");
    }
    await sleep(700);

    sendCommand(frame, cmdId("CmdToggleKeyboardHelp"));
    const help = await waitForHelpWindow(pid, true);
    if (!help) {
      throw new Error("issue-5958: keyboard help window was not created");
    }
    const r = getWindowRect(help);
    if (r.right - r.left < 100 || r.bottom - r.top < 100) {
      throw new Error(`issue-5958: keyboard help window is degenerate: ${JSON.stringify(r)}`);
    }
    // a few pixels in from the left edge: window background, no text
    const shown = screenPixels(r, 3);

    sendCommand(frame, cmdId("CmdToggleKeyboardHelp"));
    const still = await waitForHelpWindow(pid, false);
    if (still) {
      throw new Error("issue-5958: keyboard help window did not close");
    }
    await sleep(400);
    const hidden = screenPixels(r, 3);

    console.log(`issue-5958 help rect ${JSON.stringify(r)}`);
    console.log(`  shown : ${fmtRow(shown)}`);
    console.log(`  hidden: ${fmtRow(hidden)}`);

    if (sameRow(shown, hidden)) {
      throw new Error(
        `issue-5958: screen under the keyboard help window never changed, so it was never painted:\n` +
          `  shown : ${fmtRow(shown)}\n  hidden: ${fmtRow(hidden)}`,
      );
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
