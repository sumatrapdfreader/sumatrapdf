// #6045: the in-app Manual keeps its themed document background and text
// colors after the window is resized or maximized.
//
// Needs WebView2. If the documentation window never appears, skip.
//
// Run: bun tests/issue-6045.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  captureWindowPixels,
  clientToScreen,
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  moveWindow,
  setForegroundWindow,
  setProcessDpiAware,
  showWindow,
  sleep,
  SW_MAXIMIZE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const HELP_TITLE = "SumatraPDF Documentation";

function findHelpWindow(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getWindowText(hwnd) === HELP_TITLE) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function findWebViewHost(help: number): number {
  let found = 0;
  enumChildWindows(help, (hwnd) => {
    if (getClassName(hwnd) === "SumatraWgDefaultWinClass") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForHelpWebView(pid: number, timeoutMs: number): Promise<{ help: number; webView: number } | null> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() <= deadline) {
    const help = findHelpWindow(pid);
    const webView = help ? findWebViewHost(help) : 0;
    if (help && webView && isWindowVisible(help) && isWindowVisible(webView)) {
      return { help, webView };
    }
    await sleep(50);
  }
  return null;
}

function nearBlackFraction(pixels: Uint8Array): number {
  let black = 0;
  for (let i = 0; i < pixels.length; i += 4) {
    if (pixels[i] <= 5 && pixels[i + 1] <= 5 && pixels[i + 2] <= 5) {
      black++;
    }
  }
  return black / (pixels.length / 4);
}

function captureWebViewRegion(
  help: number,
  webView: number,
  x: number,
  y: number,
  dx: number,
  dy: number,
): Uint8Array | null {
  const capture = captureWindowPixels(help);
  const webRect = getWindowRect(webView);
  const clientOrigin = clientToScreen(help, 0, 0);
  x += webRect.left - clientOrigin.x;
  y += webRect.top - clientOrigin.y;
  if (!capture || x < 0 || y < 0 || x + dx > capture.w || y + dy > capture.h) {
    return null;
  }
  const pixels = new Uint8Array(dx * dy * 4);
  for (let row = 0; row < dy; row++) {
    const srcStart = ((y + row) * capture.w + x) * 4;
    const dstStart = row * dx * 4;
    pixels.set(capture.data.subarray(srcStart, srcStart + dx * 4), dstStart);
  }
  return pixels;
}

async function waitForDarkTheme(help: number, webView: number, where: string): Promise<void> {
  const deadline = Date.now() + 3000;
  let fraction = 0;
  let firstPixel = "";
  do {
    const r = getWindowRect(webView);
    const dx = r.right - r.left;
    const dy = r.bottom - r.top;
    const pixels = captureWebViewRegion(help, webView, Math.max(0, dx - 140), Math.max(0, dy - 100), 100, 60);
    if (!pixels) {
      throw new Error(`issue-6045: failed to read the ${where} Manual background`);
    }
    firstPixel = `${pixels[2]},${pixels[1]},${pixels[0]}`;
    fraction = nearBlackFraction(pixels);
    if (fraction >= 0.98) {
      return;
    }
    await sleep(50);
  } while (Date.now() <= deadline);
  throw new Error(
    `issue-6045: ${where} Manual background was only ${(fraction * 100).toFixed(1)}% theme black (first RGB ${firstPixel})`,
  );
}

async function waitForLightThemeText(help: number, webView: number): Promise<void> {
  const deadline = Date.now() + 15000;
  let lightPixels = 0;
  do {
    // Maximizing makes the left navigation visible. It contains many rows of
    // ordinary text; start below the white search input at its top.
    const r = getWindowRect(webView);
    const sampleDy = Math.min(500, r.bottom - r.top - 85);
    const pixels = captureWebViewRegion(help, webView, 15, 75, 190, sampleDy);
    if (!pixels) {
      throw new Error("issue-6045: failed to read the Manual text");
    }
    lightPixels = 0;
    for (let i = 0; i < pixels.length; i += 4) {
      if (pixels[i] >= 180 && pixels[i + 1] >= 180 && pixels[i + 2] >= 180) {
        lightPixels++;
      }
    }
    if (lightPixels >= 100) {
      return;
    }
    await sleep(50);
  } while (Date.now() <= deadline);
  throw new Error(`issue-6045: Manual text did not follow the light Dark-theme text color (${lightPixels} pixels)`);
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const appdata = tmpPath("issue-6045-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "Theme = Dark\nRestoreSession = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata], { defaultWindowPos: true });
  try {
    sendCommandSync(frame, cmdId("CmdHelpOpenManual"));
    const windows = await waitForHelpWebView(proc.pid!, 15000);
    if (!windows) {
      console.log("SKIP issue-6045: Help: Manual window did not open (WebView2 missing?)");
      return;
    }

    moveWindow(windows.help, 50, 50, 720, 560, true);
    setForegroundWindow(windows.help);
    await waitForDarkTheme(windows.help, windows.webView, "resized");

    showWindow(windows.help, SW_MAXIMIZE);
    await waitForDarkTheme(windows.help, windows.webView, "maximized");
    await waitForLightThemeText(windows.help, windows.webView);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
