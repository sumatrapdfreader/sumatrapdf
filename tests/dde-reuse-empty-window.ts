// DDE Open with newWindow=1 must reuse a window that shows only the home page
// instead of creating a second window.

import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { runStandalone } from "./util.ts";
import { enumWindows, getClassName, getWindowPid, getWindowText, sendCopyDataW, sleep } from "./winapi.ts";
import { FRAME_CLASS, killAndWait, launchControlled } from "./win-automation.ts";

const kCopyDataDdeW = 0x44646557;

// Relayout sends WM_SETREDRAW, which clears WS_VISIBLE until it finishes.
// The frame is still the one window; only a second frame, or this one
// going away, means the empty window was not reused.
function countFrames(pid: number): number {
  let n = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === FRAME_CLASS) {
      n++;
    }
    return true;
  });
  return n;
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("dde-reuse-empty-window: skipped (Windows only)");
    return;
  }
  const pdf = join(import.meta.dir, "issue-1189.pdf");
  const { proc, client, frame } = await launchControlled([]);
  try {
    if (!sendCopyDataW(frame, kCopyDataDdeW, `[Open("${pdf}", 1, 1, 0)]`)) {
      throw new Error("dde-reuse-empty-window: DDE Open failed");
    }
    // the document opens in the existing window, never in a second one
    for (let i = 0; ; i++) {
      const n = countFrames(proc.pid!);
      if (n !== 1) {
        throw new Error(`dde-reuse-empty-window: expected 1 window, got ${n}`);
      }
      if (getWindowText(frame).includes("issue-1189")) {
        break;
      }
      if (i >= 100) {
        throw new Error("dde-reuse-empty-window: document did not open");
      }
      await sleep(50);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("dde-reuse-empty-window: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
