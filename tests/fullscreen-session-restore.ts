// A session saved in fullscreen from a normal (not maximized) window: after a
// restart, leaving fullscreen must bring back that normal window, not maximize.
// Session restore showed the window maximized before entering fullscreen, and
// that maximized state is what exiting fullscreen went back to.
//
// Run: bun tests/fullscreen-session-restore.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { getWindowRect, isZoomed, postMessage, sleep, WM_CLOSE, type Rect } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const WIN_STATE_FULLSCREEN = 3;

const sameRect = (a: Rect, b: Rect) => JSON.stringify(a) === JSON.stringify(b);

// the frame's rect once it stops changing
async function settledRect(hwnd: number): Promise<Rect> {
  const deadline = Date.now() + 5000;
  let prev = getWindowRect(hwnd);
  for (;;) {
    await sleep(100);
    const cur = getWindowRect(hwnd);
    if (sameRect(cur, prev) || Date.now() >= deadline) {
      return cur;
    }
    prev = cur;
  }
}

async function toggleFullscreen(frame: number): Promise<Rect> {
  sendCommand(frame, cmdId("CmdToggleFullscreen"));
  await sleep(300);
  return settledRect(frame);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("fullscreen-session-restore");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const settingsPath = join(dir, "SumatraPDF-settings.txt");
  writeFileSync(
    settingsPath,
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = true\nReuseInstance = false\n" +
      "WindowState = 1\nWindowPos = 560 120 800 860\n",
  );

  // 1: a normal window with a document, fullscreen, quit
  const first = await launchControlled(["-appdata", dir, PDF], { defaultWindowPos: true, saveSettings: true });
  let normal: Rect;
  try {
    await first.client.waitForRenderIdle();
    normal = await settledRect(first.frame);
    if (isZoomed(first.frame)) {
      throw new Error("fullscreen-session-restore: the first window started maximized");
    }
    await toggleFullscreen(first.frame);
    postMessage(first.frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(first.proc))) {
      throw new Error("fullscreen-session-restore: the first run didn't exit");
    }
  } finally {
    first.client.close();
    await killAndWait(first.proc);
  }
  const settings = readFileSync(settingsPath, "utf8");
  if (!new RegExp(`^\\s*WindowState = ${WIN_STATE_FULLSCREEN}$`, "m").test(settings)) {
    throw new Error(`fullscreen-session-restore: fullscreen wasn't saved:\n${settings}`);
  }

  // 2: the restored session is fullscreen; leaving it must restore the normal window
  const second = await launchControlled(["-appdata", dir], { defaultWindowPos: true, saveSettings: true });
  try {
    await second.client.waitForRenderIdle();
    await settledRect(second.frame);
    const after = await toggleFullscreen(second.frame);
    const state = `zoomed=${isZoomed(second.frame)} rect ${JSON.stringify(after)}, want ${JSON.stringify(normal)}`;
    if (isZoomed(second.frame) || !sameRect(after, normal)) {
      throw new Error(`fullscreen-session-restore: leaving fullscreen didn't restore the normal window: ${state}`);
    }
    postMessage(second.frame, WM_CLOSE, 0, 0);
    await waitForExit(second.proc);
  } finally {
    second.client.close();
    await killAndWait(second.proc);
  }
  console.log("fullscreen-session-restore: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
