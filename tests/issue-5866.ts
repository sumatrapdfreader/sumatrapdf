// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5866
//
// Leaving full screen (F11) left parts of the document painted over the caption
// row: the tab bar and toolbar kept whatever pixels were on screen, so the page
// showed through where the tabs belong. Switching tabs cleared it; scrolling,
// Alt+Tab and another F11 pair did not.
//
// Two conditions are needed. The window must be MAXIMIZED, and the monitor work
// area must equal the whole screen — i.e. the taskbar set to auto-hide. Then a
// maximized window's client rect matches the full-screen one, and ExitFullScreen
// takes its `HwndClientRect(frame) == cr` branch, calling RelayoutFrame directly.
// A later relayout then opened its own WM_SETREDRAW FALSE window, which discards
// update regions already pending on the tab bar and toolbar, and its closing
// RedrawWindow lacked RDW_ALLCHILDREN, so nothing ever invalidated them again.
//
// So this test toggles taskbar auto-hide on and RESTORES it in a finally. The
// chrome geometry is identical before and after; only the pixels differ, so the
// assertion compares a sparse sample of the top chrome rows of the frame DC.
//
// Pixel compare skips GetPixel CLR_INVALID (0xffffffff): concurrent DWM/paint
// makes some samples fail even when the chrome looks correct; that used to make
// this test fail while nothing was wrong on screen.
//
// Keep this fast: F11 is near-instant; do not pad with multi-second sleeps or
// dense GetPixel grids (a 200px × every-6px strip was ~64k FFI calls × 2).
//
// Not registered in any suite, on purpose: it flips the taskbar's auto-hide
// setting on the machine it runs on (restored in a finally, but a crashed run
// would leave it), it is the slowest of the regular tests at ~12s, and a
// hosted runner has no Explorer taskbar, so the daily could never run it
// anyway. Run it by hand when touching fullscreen / relayout / chrome paint.
//
// Run:  bun tests/issue-5866.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, tmpPath, EXE } from "./util.ts";
import { countDifferingPixels, sendCommand, waitForFrame, killAndWait } from "./win-automation.ts";
import {
  getTaskbarAutoHide,
  getWindowRect,
  postMessage,
  readWindowDCColumn,
  setProcessDpiAware,
  setTaskbarAutoHide,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
// Caption + tabs-in-titlebar + toolbar. Deep enough for the bug, shallow enough
// to stay out of the document canvas (where progressive render is noisy).
const STRIP_DY = 80;
const X_STEP = 16;
// DWM occasionally changes a handful of anti-aliased caption pixels between
// otherwise identical paints. The regression leaves thousands different.
const MAX_CHANGED_PIXELS = 20;

// WindowState = 2 is maximized, the other half of the repro
const SETTINGS = `WindowState = 2
CheckForUpdates = false
RestoreSession = false
UiLanguage = en
`;

// Sparse sample of the top chrome strip (far fewer GetPixel calls than
// topChromePixels with STRIP_DY=200 / step 6).
function sampleChrome(hwnd: number): number[] {
  const r = getWindowRect(hwnd);
  const w = r.right - r.left;
  const px: number[] = [];
  for (let x = 4; x < w - 4; x += X_STEP) {
    px.push(...readWindowDCColumn(hwnd, x, 0, STRIP_DY));
  }
  return px;
}

// Chrome pixels once they stop changing: a slow build (ASan, a loaded CI
// runner) is still painting when a fixed sleep expires, and a half-painted
// "before" makes every later comparison differ.
async function sampleStableChrome(hwnd: number, timeoutMs = 5000): Promise<number[]> {
  const deadline = Date.now() + timeoutMs;
  let prev = sampleChrome(hwnd);
  for (;;) {
    await sleep(120);
    const cur = sampleChrome(hwnd);
    if (countDifferingPixels(prev, cur) === 0 || Date.now() > deadline) {
      return cur;
    }
    prev = cur;
  }
}

// How many sampled pixels still differ from `before`, once they stop coming
// back (or the deadline passes). The bug never repaints, so it still fails --
// just after waiting instead of after a fixed sleep that a slow build misses.
async function waitForChromeRestored(hwnd: number, before: number[], timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const nDiff = countDifferingPixels(before, sampleChrome(hwnd));
    if (nDiff <= MAX_CHANGED_PIXELS || Date.now() > deadline) {
      return nDiff;
    }
    await sleep(120);
  }
}

export async function testit(): Promise<void> {
  // window rects and window-DC pixel coordinates must agree; no-op at 100% DPI
  setProcessDpiAware();

  const appDataDir = tmpPath("issue-5866-appdata");
  rmSync(appDataDir, { recursive: true, force: true });
  mkdirSync(appDataDir, { recursive: true });
  writeFileSync(join(appDataDir, "SumatraPDF-settings.txt"), SETTINGS);

  const hadAutoHide = getTaskbarAutoHide();
  let proc: Bun.Subprocess | undefined;
  let frame = 0;
  try {
    // SHAppBarMessage takes effect immediately; no need to wait on Explorer
    setTaskbarAutoHide(true);

    proc = Bun.spawn([EXE, "-for-testing", "-appdata", appDataDir, PDF], {
      stdout: "ignore",
      stderr: "ignore",
    });
    frame = await waitForFrame(proc.pid!, 8000);
    if (!frame) {
      throw new Error("SumatraPDF frame window not found");
    }
    // settle for first paint / maximized layout (not full-page render)
    const before = await sampleStableChrome(frame);

    sendCommand(frame, cmdId("CmdToggleFullscreen"));
    await sleep(200);
    sendCommand(frame, cmdId("CmdToggleFullscreen"));

    // nothing here scrolls, switches tabs or moves the mouse: leaving full
    // screen must repaint the chrome on its own
    const nDiff = await waitForChromeRestored(frame, before);
    if (nDiff > MAX_CHANGED_PIXELS) {
      throw new Error(
        `the caption/tab row did not repaint after leaving full screen: ` +
          `${nDiff} of ${before.length} sampled pixels still differ from before`,
      );
    }
  } finally {
    if (frame) {
      postMessage(frame, WM_CLOSE, 0, 0);
      await sleep(100);
    }
    await killAndWait(proc);
    setTaskbarAutoHide(hadAutoHide);
    rmSync(appDataDir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-5866");
}
