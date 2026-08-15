// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5850
//
// With Scrollbars = windows, UseTabs = false and a maximized window, opening a
// document from a cold start left the canvas's vertical scrollbar as a blank
// strip: no thumb and no arrows until the first scroll or a hover.
//
// Cause: RelayoutFrame suppresses intermediate repaints with WM_SETREDRAW FALSE,
// which clears WS_VISIBLE on the frame. The relayout that follows SW_MAXIMIZE
// resizes the canvas, whose WM_SIZE reconfigures the scrollbars -- but with the
// frame "hidden" the non-client paint is discarded. The redraw afterwards only
// invalidated the canvas's *client* area, so the scrollbar was never drawn.
//
// The scrollbar lives in the non-client area, so this reads pixels from the
// canvas's window DC (PrintWindow renders the NC area blank). A painted bar has
// a track plus a thumb; the bug shows up as a single flat color.
//
// Run:  bun tests/issue-5850.ts [--no-build]   (or via tests/all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, tmpPath } from "./util.ts";
import { launchSumatra, waitForFrame, findCanvas, vScrollbarColorCount } from "./win-automation.ts";
import { setProcessDpiAware, sleep, postMessage, WM_CLOSE } from "./winapi.ts";

// zlib.3.pdf is 2 pages, so at the default "fit page" zoom it needs a vertical
// scrollbar in a maximized window
const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

// All three matter: with tabs on, or in a non-maximized window, the scrollbar
// painted correctly even before the fix.
const SETTINGS = `Scrollbars = windows
UseTabs = false
RestoreSession = false
CheckForUpdates = false
WindowState = 2
WindowPos = 200 100 1000 800
`;

export async function testit(): Promise<void> {
  // window rects and window-DC pixel coordinates must agree; no-op at 100% DPI
  setProcessDpiAware();

  // -appdata keeps this out of both the user's %APPDATA% and the portable
  // out/dbg64/SumatraPDF-settings.txt that a manual run may have left behind
  const appDataDir = tmpPath("issue-5850-appdata");
  rmSync(appDataDir, { recursive: true, force: true });
  mkdirSync(appDataDir, { recursive: true });
  writeFileSync(join(appDataDir, "SumatraPDF-settings.txt"), SETTINGS);

  const proc = launchSumatra(["-appdata", appDataDir, PDF], { defaultWindowPos: true });
  let frame = 0;
  try {
    frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error("SumatraPDF frame window not found");
    }
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("SumatraPDF canvas window not found");
    }
    // let the document render and the startup relayout settle. Nothing here
    // scrolls or moves the mouse: the scrollbar must be painted on its own.
    await sleep(2500);

    const nColors = vScrollbarColorCount(canvas);
    if (nColors < 2) {
      throw new Error(
        `vertical scrollbar was not painted: its column is a single flat color ` +
          `(expected a track and a thumb, got ${nColors} distinct color(s))`,
      );
    }
  } finally {
    if (frame) {
      postMessage(frame, WM_CLOSE, 0, 0);
      await sleep(500);
    }
    proc.kill();
    rmSync(appDataDir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-5850");
}
