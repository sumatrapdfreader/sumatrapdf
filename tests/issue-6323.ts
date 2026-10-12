// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6323
//
// WebView2's JavaScript print path showed a browser preview whose printer
// selection did not work. Markdown printing must use its system print dialog.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

function visibleWindows(): string {
  const windows: string[] = [];
  enumWindows((hwnd) => {
    if (isWindowVisible(hwnd)) {
      windows.push(`${getWindowPid(hwnd)} ${getClassName(hwnd)} ${getWindowText(hwnd)}`);
    }
    return true;
  });
  return windows.join("\n");
}

function topWindows(): Set<number> {
  const windows = new Set<number>();
  enumWindows((hwnd) => {
    if (isWindowVisible(hwnd)) {
      windows.add(hwnd);
    }
    return true;
  });
  return windows;
}

function newPrintDialog(before: Set<number>): number {
  let dialog = 0;
  enumWindows((hwnd) => {
    if (before.has(hwnd) || !isWindowVisible(hwnd)) {
      return true;
    }
    const cls = getClassName(hwnd);
    if (cls === "#32770" || cls === "ApplicationFrameWindow") {
      dialog = hwnd;
      return false;
    }
    return true;
  });
  return dialog;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("  skipped: original Windows WebView2 printing only");
    return;
  }

  const dir = tmpPath("issue-6323");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const md = join(dir, "document.md");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata);
  writeFileSync(md, "# Printer selection\n\nMarkdown print test.\n");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["MarkdownUI [", "\tUseFixedPageUI = false", "]", "RestoreSession = false", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, md]);
  try {
    await client.waitForRenderIdle();
    const before = topWindows();
    sendCommand(frame, cmdId("CmdPrint"));
    let dialog = 0;
    for (const deadline = Date.now() + 8000; Date.now() < deadline && !dialog;) {
      dialog = newPrintDialog(before);
      await sleep(100);
    }
    if (!dialog) {
      throw new Error(`issue-6323: Markdown printing did not open the system printer dialog\n${visibleWindows()}`);
    }
    postMessage(dialog, WM_CLOSE, 0, 0);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
