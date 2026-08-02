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
// assertion compares a dense sample of the top rows of the frame's window DC.
//
// Run:  bun tests/issue-5866.ts [--no-build]   (or via tests/all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, tmpPath } from "./util.ts";
import { countDifferingPixels, sendCommand, topChromePixels, waitForFrame } from "./win-automation.ts";
import {
  getTaskbarAutoHide,
  postMessage,
  setProcessDpiAware,
  setTaskbarAutoHide,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";
import { EXE } from "./util.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
// caption row + tab bar + toolbar; the leftovers land in this band
const STRIP_DY = 200;

// WindowState = 2 is maximized, the other half of the repro
const SETTINGS = `WindowState = 2
CheckForUpdates = false
RestoreSession = false
UiLanguage = en
`;

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
    setTaskbarAutoHide(true);
    await sleep(600); // let the work area change land

    proc = Bun.spawn([EXE, "-appdata", appDataDir, PDF], { stdout: "ignore", stderr: "ignore" });
    frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error("SumatraPDF frame window not found");
    }
    await sleep(3000); // document render + startup layout

    const before = topChromePixels(frame, STRIP_DY);

    sendCommand(frame, cmdId("CmdToggleFullscreen"));
    await sleep(1800);
    sendCommand(frame, cmdId("CmdToggleFullscreen"));
    await sleep(2500);

    // nothing here scrolls, switches tabs or moves the mouse: leaving full
    // screen must repaint the chrome on its own
    const nDiff = countDifferingPixels(before, topChromePixels(frame, STRIP_DY));
    if (nDiff > 0) {
      throw new Error(
        `the caption/tab row did not repaint after leaving full screen: ` +
          `${nDiff} of ${before.length} sampled pixels still differ from before`,
      );
    }
  } finally {
    if (frame) {
      postMessage(frame, WM_CLOSE, 0, 0);
      await sleep(600);
    }
    proc?.kill();
    setTaskbarAutoHide(hadAutoHide);
    rmSync(appDataDir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-5866");
}
