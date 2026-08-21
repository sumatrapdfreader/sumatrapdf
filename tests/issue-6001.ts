// #6001: modal dialogs must be owned top-level windows, not children of the
// frame that RunModalWindow disables.

import { mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  enumWindows,
  findChildWindow,
  getClassName,
  getWindowOwner,
  getWindowPid,
  isWindowEnabled,
  isWindowVisible,
  postMessage,
  sendText,
  sleep,
  VK_RETURN,
  WM_CLOSE,
  WM_KEYDOWN,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const DEFAULT_WINDOW_CLASS = "SumatraWgDefaultWinClass";

function topWindows(pid: number): Set<number> {
  const result = new Set<number>();
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid) {
      result.add(hwnd);
    }
    return true;
  });
  return result;
}

function findNewVisibleWindow(pid: number, before: Set<number>, exclude = 0): number {
  let result = 0;
  enumWindows((hwnd) => {
    if (
      hwnd !== exclude &&
      !before.has(hwnd) &&
      getWindowPid(hwnd) === pid &&
      getClassName(hwnd) === DEFAULT_WINDOW_CLASS &&
      isWindowVisible(hwnd)
    ) {
      result = hwnd;
      return false;
    }
    return true;
  });
  return result;
}

async function waitForNewVisibleWindow(pid: number, before: Set<number>, exclude = 0): Promise<number> {
  const deadline = Date.now() + 3000;
  while (Date.now() < deadline) {
    const hwnd = findNewVisibleWindow(pid, before, exclude);
    if (hwnd) {
      return hwnd;
    }
    await sleep(40);
  }
  return 0;
}

async function waitForClosed(pid: number, hwnd: number): Promise<void> {
  const deadline = Date.now() + 3000;
  while (Date.now() < deadline) {
    if (!topWindows(pid).has(hwnd)) {
      return;
    }
    await sleep(40);
  }
  throw new Error(`issue-6001: modal window ${hwnd} did not close`);
}

// the dialog is shown before RunModalWindow disables the owner, and we see it
// from another process, so poll instead of sampling the instant it appears
async function waitForEnabled(frame: number, want: boolean): Promise<boolean> {
  const deadline = Date.now() + 3000;
  for (;;) {
    if (isWindowEnabled(frame) === want) {
      return true;
    }
    if (Date.now() >= deadline) {
      return false;
    }
    await sleep(20);
  }
}

async function checkModal(pid: number, frame: number, hwnd: number, command: string): Promise<void> {
  if (!hwnd) {
    throw new Error(`issue-6001: ${command} did not create a visible top-level dialog`);
  }
  if (getWindowOwner(hwnd) !== frame) {
    throw new Error(`issue-6001: ${command} dialog is not owned by the frame`);
  }
  if (!(await waitForEnabled(frame, false))) {
    throw new Error(`issue-6001: ${command} did not disable its owner`);
  }
  postMessage(hwnd, WM_CLOSE, 0, 0);
  await waitForClosed(pid, hwnd);
  if (!(await waitForEnabled(frame, true))) {
    throw new Error(`issue-6001: ${command} did not re-enable its owner`);
  }
}

async function openDirect(pid: number, frame: number, command: string): Promise<void> {
  const before = topWindows(pid);
  sendCommand(frame, cmdId(command));
  const dialog = await waitForNewVisibleWindow(pid, before);
  await checkModal(pid, frame, dialog, command);
}

async function openScreenshotFromPalette(
  pid: number,
  frame: number,
  client: Awaited<ReturnType<typeof launchControlled>>["client"],
): Promise<void> {
  const beforePalette = topWindows(pid);
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const palette = await waitForNewVisibleWindow(pid, beforePalette);
  if (!palette) {
    throw new Error("issue-6001: command palette did not open");
  }
  const edit = findChildWindow(palette, "Edit");
  if (!edit) {
    throw new Error("issue-6001: command palette has no query edit");
  }

  sendText(edit, "Set Screenshot Hotkey");
  const expected = cmdId("CmdSetScreenshotHotkey");
  const deadline = Date.now() + 3000;
  for (;;) {
    const response = await client.request(ControlCommand.TestCommandPalette, []);
    if (String(response[1] ?? "").includes(`cmd=${expected}`)) {
      break;
    }
    if (Date.now() >= deadline) {
      throw new Error(`issue-6001: command palette did not select CmdSetScreenshotHotkey: ${response}`);
    }
    await sleep(40);
  }

  const beforeDialog = topWindows(pid);
  postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
  const dialog = await waitForNewVisibleWindow(pid, beforeDialog, palette);
  await checkModal(pid, frame, dialog, "CmdSetScreenshotHotkey from the command palette");
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-6001");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appData, pdf]);
  try {
    await client.waitForRenderIdle();
    for (const command of [
      "CmdFavoriteAdd",
      "CmdChangeLanguage",
      "CmdChangeScrollbar",
      "CmdZoomCustom",
      "CmdSetInverseSearch",
    ]) {
      await openDirect(proc.pid!, frame, command);
    }
    // Go to page uses the toolbar's page box in normal mode. Presentation mode
    // exercises its RunModalWindow fallback.
    sendCommand(frame, cmdId("CmdTogglePresentationMode"));
    await sleep(200);
    await openDirect(proc.pid!, frame, "CmdGoToPage");
    sendCommand(frame, cmdId("CmdTogglePresentationMode"));
    await openScreenshotFromPalette(proc.pid!, frame, client);
    console.log("issue-6001: modal dialogs are owned, usable top-level windows");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
