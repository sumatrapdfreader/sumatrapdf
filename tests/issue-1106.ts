// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1106
//
// Rotating the display while Sumatra is fullscreen left the restore geometry
// stale: exiting fullscreen reapplied a pre-rotation maximized rect so the
// window looked "maximized" but did not cover the work area (title-bar buttons
// off-screen on Win7 tablets).
//
// We cannot rotate the physical display in CI. This checks the two pieces we
// can: (1) exit fullscreen after a maximized entry re-maximizes correctly, and
// (2) WM_DISPLAYCHANGE while fullscreen keeps the frame covering the monitor
// and still re-maximizes on exit.
//
// Run:  bun tests/issue-1106.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, tmpPath } from "./util.ts";
import { launchSumatra, sendCommandSync, waitForExit, waitForFrame, killAndWait } from "./win-automation.ts";
import {
  getWindowRect,
  isZoomed,
  postMessage,
  sendMessage,
  setProcessDpiAware,
  sleep,
  WM_CLOSE,
  WM_DISPLAYCHANGE,
} from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

const SETTINGS = `RestoreSession = false
CheckForUpdates = false
WindowState = 2
WindowPos = 200 100 1000 800
`;

function rectSize(r: { left: number; top: number; right: number; bottom: number }) {
  return { w: r.right - r.left, h: r.bottom - r.top };
}

// the window's size once it stops changing
async function settledSize(hwnd: number): Promise<{ w: number; h: number }> {
  const deadline = Date.now() + 5000;
  let prev = rectSize(getWindowRect(hwnd));
  for (;;) {
    await sleep(50);
    const cur = rectSize(getWindowRect(hwnd));
    if ((cur.w === prev.w && cur.h === prev.h) || Date.now() >= deadline) {
      return cur;
    }
    prev = cur;
  }
}

function nearlySameSize(a: { w: number; h: number }, b: { w: number; h: number }, tol = 8): boolean {
  return Math.abs(a.w - b.w) <= tol && Math.abs(a.h - b.h) <= tol;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();

  const appDataDir = tmpPath("issue-1106-appdata");
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
    const deadline0 = Date.now() + 5000;
    while (!isZoomed(frame) && Date.now() < deadline0) {
      await sleep(30);
    }
    if (!isZoomed(frame)) {
      throw new Error("expected window to start maximized (WindowState = 2)");
    }
    // WM_SIZE reports the maximized state before the frame has been resized to
    // the work area, so sampling right away can capture the pre-maximize
    // WindowPos size and make the comparisons below meaningless
    const maximizedBefore = await settledSize(frame);

    // --- path 1: maximized → fullscreen → exit → re-maximized ---
    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    const fsSize = rectSize(getWindowRect(frame));
    if (fsSize.w < 640 || fsSize.h < 480) {
      throw new Error(`fullscreen did not expand the frame: ${fsSize.w}x${fsSize.h}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));

    if (!isZoomed(frame)) {
      throw new Error("after leaving fullscreen, window should be maximized again");
    }
    const afterExit1 = rectSize(getWindowRect(frame));
    if (!nearlySameSize(afterExit1, maximizedBefore)) {
      throw new Error(
        `restored maximize size wrong: got ${afterExit1.w}x${afterExit1.h}, expected ~${maximizedBefore.w}x${maximizedBefore.h}`,
      );
    }

    // --- path 2: fullscreen + WM_DISPLAYCHANGE → still FS → exit → maximized ---
    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    const fsBefore = rectSize(getWindowRect(frame));

    // wParam = bits/pixel (unused); lParam packs cx/cy of the new mode
    sendMessage(frame, WM_DISPLAYCHANGE, 32, (fsBefore.h << 16) | (fsBefore.w & 0xffff));

    const fsAfter = rectSize(getWindowRect(frame));
    if (!nearlySameSize(fsAfter, fsBefore, 4)) {
      throw new Error(
        `WM_DISPLAYCHANGE changed fullscreen size unexpectedly: ${fsAfter.w}x${fsAfter.h} vs ${fsBefore.w}x${fsBefore.h}`,
      );
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));

    if (!isZoomed(frame)) {
      throw new Error("after display-change + exit fullscreen, window should be maximized");
    }
    const afterExit2 = rectSize(getWindowRect(frame));
    if (!nearlySameSize(afterExit2, maximizedBefore)) {
      throw new Error(
        `maximize after display-change wrong: got ${afterExit2.w}x${afterExit2.h}, expected ~${maximizedBefore.w}x${maximizedBefore.h}`,
      );
    }
  } finally {
    if (frame) {
      postMessage(frame, WM_CLOSE, 0, 0);
      await waitForExit(proc, 5000);
    }
    await killAndWait(proc);
    rmSync(appDataDir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-1106");
}
