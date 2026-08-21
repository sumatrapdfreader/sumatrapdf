// #5988: Esc must close the image editor opened after choosing a screenshot.

import { mkdirSync, rmSync } from "node:fs";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { findTopWindow, postMessage, sleep, VK_ESCAPE, VK_RETURN, waitForTopWindow, WM_KEYDOWN } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const SCREENSHOT_OVERLAY_CLASS = "SumatraScreenshotOverlay";
const IMAGE_EDIT_CLASS = "SUMATRA_PDF_IMAGE_EDIT";

async function waitForGone(pid: number, className: string, timeoutMs = 3000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (!findTopWindow(pid, className)) {
      return;
    }
    await sleep(40);
  }
  throw new Error(`issue-5988: ${className} stayed open after Esc`);
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-5988");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });

  const { proc, client, frame } = await launchControlled(["-appdata", appData]);
  try {
    sendCommand(frame, cmdId("CmdScreenshot"));
    const overlay = await waitForTopWindow(proc.pid!, SCREENSHOT_OVERLAY_CLASS);
    if (!overlay) {
      throw new Error("issue-5988: screenshot picker did not open");
    }

    postMessage(overlay, WM_KEYDOWN, VK_RETURN, 0);
    const editor = await waitForTopWindow(proc.pid!, IMAGE_EDIT_CLASS);
    if (!editor) {
      throw new Error("issue-5988: image editor did not open");
    }

    postMessage(editor, WM_KEYDOWN, VK_ESCAPE, 0);
    await waitForGone(proc.pid!, IMAGE_EDIT_CLASS);
    console.log("issue-5988: screenshot image editor closed with Esc");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
