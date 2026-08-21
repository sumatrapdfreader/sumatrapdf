// #5989: cancelling the screenshot picker must restore document keyboard focus.

import { mkdirSync, rmSync } from "node:fs";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  findTopWindow,
  getClassName,
  getFocusedHwnd,
  postMessage,
  sleep,
  VK_ESCAPE,
  waitForTopWindow,
  WM_KEYDOWN,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const SCREENSHOT_OVERLAY_CLASS = "SumatraScreenshotOverlay";

async function waitForPickerGone(pid: number, timeoutMs = 3000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (!findTopWindow(pid, SCREENSHOT_OVERLAY_CLASS)) {
      return;
    }
    await sleep(40);
  }
  throw new Error("issue-5989: screenshot picker stayed open after Esc");
}

async function waitForDocumentFocus(frame: number, timeoutMs = 3000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let focused = 0;
  while (Date.now() < deadline) {
    focused = getFocusedHwnd(frame);
    if (focused === frame) {
      return;
    }
    await sleep(40);
  }
  const focusedClass = focused ? getClassName(focused) : "none";
  throw new Error(`issue-5989: document focus was not restored (focused class: ${focusedClass})`);
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-5989");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });

  const { proc, client, frame } = await launchControlled(["-appdata", appData]);
  try {
    await waitForDocumentFocus(frame);

    sendCommand(frame, cmdId("CmdScreenshot"));
    const overlay = await waitForTopWindow(proc.pid!, SCREENSHOT_OVERLAY_CLASS);
    if (!overlay) {
      throw new Error("issue-5989: screenshot picker did not open");
    }

    postMessage(overlay, WM_KEYDOWN, VK_ESCAPE, 0);
    await waitForPickerGone(proc.pid!);
    await waitForDocumentFocus(frame);
    console.log("issue-5989: cancelling screenshot picker restored document focus");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
