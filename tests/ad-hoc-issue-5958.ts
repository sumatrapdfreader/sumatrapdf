// Ad-hoc regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5958
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
// That is also why this is ad-hoc rather than part of run-almost-all: reading
// the real screen means anything that covers that spot -- another window, a
// notification, someone using the machine while the suite runs -- fails the
// test, and a white window over a white background reads as "never painted"
// even when it did paint. Run it on an idle desktop when touching the help
// window or WindowBase visibility.
//
// Run:  bun tests/ad-hoc-issue-5958.ts [--no-build]

import { cmdId, runStandalone } from "./util.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  readWindowDCRow,
  topLevelWindowFromPoint,
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

// Waits until the help window is (or is no longer) on screen; returns whether
// that happened. Closing it deletes the window from a queued task, so for a
// moment the hwnd still exists while already hidden -- "not visible" is what
// closed means here, and returning the hwnd instead of a verdict is how this
// test used to report that lingering window as "did not close".
async function waitForHelpShown(pid: number, want: boolean, timeoutMs = 5000): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const hwnd = findHelpWindow(pid);
    const shown = hwnd !== 0 && isWindowVisible(hwnd);
    if (shown === want) {
      return true;
    }
    if (Date.now() > deadline) {
      return false;
    }
    await sleep(100);
  }
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
    if (!(await waitForHelpShown(pid, true))) {
      throw new Error("issue-5958: keyboard help window was not created");
    }
    const help = findHelpWindow(pid);
    const r = getWindowRect(help);
    if (r.right - r.left < 100 || r.bottom - r.top < 100) {
      throw new Error(`issue-5958: keyboard help window is degenerate: ${JSON.stringify(r)}`);
    }
    // reading the screen only says anything if our window is the one on screen
    // there; anything covering it (an always-on-top window, someone using the
    // machine) looks exactly like the window not painting
    const mid = { x: r.left + 3, y: Math.floor((r.top + r.bottom) / 2) };
    const onTop = topLevelWindowFromPoint(mid.x, mid.y);
    if (onTop !== help) {
      console.log(
        `\nSKIP issue-5958: another window (hwnd ${onTop}) covers the help window at ` +
          `${mid.x},${mid.y}; run this on an idle desktop.`,
      );
      return;
    }
    // a few pixels in from the left edge: window background, no text
    const shown = screenPixels(r, 3);

    sendCommand(frame, cmdId("CmdToggleKeyboardHelp"));
    if (!(await waitForHelpShown(pid, false))) {
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
