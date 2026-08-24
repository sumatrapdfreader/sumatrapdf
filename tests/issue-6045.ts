// #6045: the in-app Manual keeps its white document background after the
// window is resized or maximized while a dark UI theme is active.
//
// Needs WebView2. If the documentation window never appears, skip.
//
// Run: bun tests/issue-6045.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  captureWindowDCRegionPixels,
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

function nearWhiteFraction(pixels: Uint8Array): number {
  let white = 0;
  for (let i = 0; i < pixels.length; i += 4) {
    if (pixels[i] >= 250 && pixels[i + 1] >= 250 && pixels[i + 2] >= 250) {
      white++;
    }
  }
  return white / (pixels.length / 4);
}

async function waitForWhiteBackground(webView: number, where: string): Promise<void> {
  const deadline = Date.now() + 3000;
  let fraction = 0;
  do {
    const r = getWindowRect(webView);
    const dx = r.right - r.left;
    const dy = r.bottom - r.top;
    const pixels = captureWindowDCRegionPixels(webView, Math.max(0, dx - 140), Math.max(0, dy - 100), 100, 60);
    if (!pixels) {
      throw new Error(`issue-6045: failed to read the ${where} Manual background`);
    }
    fraction = nearWhiteFraction(pixels);
    if (fraction >= 0.98) {
      return;
    }
    await sleep(50);
  } while (Date.now() <= deadline);
  throw new Error(`issue-6045: ${where} Manual background was only ${(fraction * 100).toFixed(1)}% white`);
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
    await waitForWhiteBackground(windows.webView, "resized");

    showWindow(windows.help, SW_MAXIMIZE);
    await waitForWhiteBackground(windows.webView, "maximized");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
