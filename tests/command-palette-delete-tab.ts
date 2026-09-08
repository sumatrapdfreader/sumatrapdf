// Delete on a command-palette tab can CloseTab, which pumps messages
// (focus change, window teardown). The palette must not use-after-free.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  enumWindows,
  getClassName,
  getFocusedHwnd,
  getRootWindow,
  getWindowPid,
  isWindowVisible,
  postMessage,
  sendText,
  sleep,
  VK_DELETE,
  VK_DOWN,
  WM_KEYDOWN,
} from "./winapi.ts";
import { FRAME_CLASS, killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

type Palette = { open: boolean; items: number };

async function paletteState(client: ControlClient): Promise<Palette> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const out = String(res[1] ?? "").trim();
  if (res[0] === 2) {
    return { open: false, items: 0 };
  }
  const m = /items=(\d+)/.exec(out);
  if (res[0] !== 0 || !m) {
    throw new Error(`command-palette-delete-tab: TestCommandPalette failed: ${out}`);
  }
  return { open: true, items: +m[1]! };
}

function findPalette(frame: number): { palette: number; edit: number } {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

function getFrames(pid: number): number[] {
  const res: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === FRAME_CLASS && isWindowVisible(hwnd)) {
      res.push(hwnd);
    }
    return true;
  });
  return res;
}

async function waitFor(client: ControlClient, what: string, pred: (p: Palette) => boolean): Promise<Palette> {
  const deadline = Date.now() + 8000;
  let last: Palette = { open: false, items: 0 };
  for (;;) {
    last = await paletteState(client);
    if (pred(last)) {
      return last;
    }
    if (Date.now() > deadline) {
      throw new Error(`command-palette-delete-tab: ${what}; palette is ${JSON.stringify(last)}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("command-palette-delete-tab");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdDuplicateInNewWindow"));

    const twoDeadline = Date.now() + 8000;
    while (getFrames(proc.pid!).length < 2 && Date.now() < twoDeadline) {
      await sleep(50);
    }
    if (getFrames(proc.pid!).length !== 2) {
      throw new Error(`command-palette-delete-tab: expected 2 frames, got ${getFrames(proc.pid!).length}`);
    }

    sendCommand(frame, cmdId("CmdCommandPalette"));
    const openDeadline = Date.now() + 8000;
    let handles = { palette: 0, edit: 0 };
    while (Date.now() < openDeadline) {
      handles = findPalette(frame);
      if (handles.palette && handles.edit) {
        break;
      }
      await sleep(50);
    }
    if (!handles.palette || !handles.edit) {
      throw new Error("command-palette-delete-tab: palette did not open");
    }

    sendText(handles.edit, "@");
    await waitFor(client, "@ did not list 4 tabs", (p) => p.open && p.items === 4);

    // Home, doc, Home, doc — Delete the first window's document
    postMessage(handles.edit, WM_KEYDOWN, VK_DOWN, 0);
    postMessage(handles.edit, WM_KEYDOWN, VK_DELETE, 0);

    const closeDeadline = Date.now() + 8000;
    let nFrames = 2;
    while (Date.now() < closeDeadline) {
      try {
        await client.request(ControlCommand.Ping, []);
      } catch (e) {
        throw new Error(`command-palette-delete-tab: process died (${String((e as Error)?.message ?? e)})`);
      }
      nFrames = getFrames(proc.pid!).length;
      if (nFrames === 1) {
        break;
      }
      await sleep(50);
    }
    if (nFrames !== 1) {
      throw new Error(`command-palette-delete-tab: expected 1 frame after Delete, got ${nFrames}`);
    }
    console.log("command-palette-delete-tab: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
