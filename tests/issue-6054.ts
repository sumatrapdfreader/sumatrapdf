// Issue #6054 / #5044: -new-window opens each file in its own window;
// -new-window-tabs opens one window and loads the files as tabs.

import { join } from "node:path";
import { ROOT, runStandalone } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled } from "./win-automation.ts";
import { enumWindows, getClassName, getWindowPid, isWindowVisible, sleep } from "./winapi.ts";

const PDF_A = join(ROOT, "tests", "issue-1189.pdf");
const PDF_B = join(ROOT, "tests", "issue-1809.pdf");

function countVisibleFrames(pid: number): number {
  let n = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === FRAME_CLASS && isWindowVisible(hwnd)) {
      n++;
    }
    return true;
  });
  return n;
}

async function waitForStableFrameCount(pid: number, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let last = 0;
  let stableAt = 0;
  while (Date.now() < deadline) {
    const n = countVisibleFrames(pid);
    if (n !== last) {
      last = n;
      stableAt = Date.now();
    } else if (n > 0 && Date.now() - stableAt >= 400) {
      return n;
    }
    await sleep(50);
  }
  return last;
}

async function countFramesForFlags(flags: string[]): Promise<number> {
  const { proc, client } = await launchControlled([...flags, PDF_A, PDF_B]);
  try {
    await client.waitForRenderIdle();
    return await waitForStableFrameCount(proc.pid!);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const each = await countFramesForFlags(["-new-window"]);
  if (each !== 2) {
    throw new Error(`issue-6054: -new-window should open 2 windows, got ${each}`);
  }
  const tabs = await countFramesForFlags(["-new-window-tabs"]);
  if (tabs !== 1) {
    throw new Error(`issue-6054: -new-window-tabs should open 1 window, got ${tabs}`);
  }
  console.log("issue-6054: -new-window → 2 windows, -new-window-tabs → 1 window");
}

if (import.meta.main) {
  await runStandalone(testit);
}
