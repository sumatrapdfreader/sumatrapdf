// #6001: modal dialogs must be owned top-level windows, not children of the
// frame that RunModalWindow disables.

import { mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  enumWindows,
  getWindowOwner,
  getWindowPid,
  isWindowEnabled,
  postMessage,
  sleep,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi.ts";
import {
  closeToolWindow,
  killAndWait,
  launchControlled,
  paletteSetText,
  sendCommand,
  waitForPalette,
  waitToolWindow,
  paletteState,
} from "./win-automation.ts";

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

type Client = Awaited<ReturnType<typeof launchControlled>>["client"];

async function waitForNewVisibleWindow(
  client: Client,
  pid: number,
  name: string,
  before: Set<number>,
  exclude = 0,
): Promise<number> {
  return waitToolWindow(client, pid, name, { before, exclude, timeoutMs: 3000 });
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

async function checkModal(
  client: Client,
  pid: number,
  frame: number,
  hwnd: number,
  name: string,
  command: string,
): Promise<void> {
  if (!hwnd) {
    throw new Error(`issue-6001: ${command} did not create a visible top-level dialog`);
  }
  if (getWindowOwner(hwnd) !== frame) {
    throw new Error(`issue-6001: ${command} dialog is not owned by the frame`);
  }
  if (!(await waitForEnabled(frame, false))) {
    throw new Error(`issue-6001: ${command} did not disable its owner`);
  }
  await closeToolWindow(client, name, hwnd);
  await waitForClosed(pid, hwnd);
  if (!(await waitForEnabled(frame, true))) {
    throw new Error(`issue-6001: ${command} did not re-enable its owner`);
  }
}

async function openDirect(client: Client, pid: number, frame: number, command: string, name: string): Promise<void> {
  const before = topWindows(pid);
  sendCommand(frame, cmdId(command));
  const dialog = await waitForNewVisibleWindow(client, pid, name, before);
  await checkModal(client, pid, frame, dialog, name, command);
}

async function openScreenshotFromPalette(pid: number, frame: number, client: Client): Promise<void> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const handles = await waitForPalette(client, frame);
  if (!handles.edit) {
    throw new Error("issue-6001: command palette did not open");
  }

  await paletteSetText(client, handles, "Set Screenshot Hotkey");
  const expected = cmdId("CmdSetScreenshotHotkey");
  const deadline = Date.now() + 3000;
  for (;;) {
    const state = await paletteState(client);
    if (state.cmd === expected) {
      break;
    }
    if (Date.now() >= deadline) {
      throw new Error(`issue-6001: command palette did not select CmdSetScreenshotHotkey: ${state.raw}`);
    }
    await sleep(40);
  }

  const beforeDialog = topWindows(pid);
  postMessage(handles.edit, WM_KEYDOWN, VK_RETURN, 0);
  const dialog = await waitForNewVisibleWindow(client, pid, "screenshothotkey", beforeDialog, handles.palette);
  await checkModal(client, pid, frame, dialog, "screenshothotkey", "CmdSetScreenshotHotkey from the command palette");
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-6001: modal ownership is a Win32 owner HWND and EnableWindow");
    return;
  }
  const appData = tmpPath("issue-6001");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appData, pdf]);
  try {
    await client.waitForRenderIdle();
    // command, and the name of its ng tool window
    for (const [command, name] of [
      ["CmdFavoriteAdd", "addfavorite"],
      ["CmdChangeLanguage", "changelanguage"],
      ["CmdChangeScrollbar", "changescrollbar"],
      ["CmdZoomCustom", "customzoom"],
      ["CmdSetInverseSearch", "inversesearch"],
    ] as const) {
      await openDirect(client, proc.pid!, frame, command, name);
    }
    // Go to page uses the toolbar's page box in normal mode. Presentation mode
    // exercises its RunModalWindow fallback.
    sendCommand(frame, cmdId("CmdTogglePresentationMode"));
    await sleep(200);
    await openDirect(client, proc.pid!, frame, "CmdGoToPage", "gotopage");
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
