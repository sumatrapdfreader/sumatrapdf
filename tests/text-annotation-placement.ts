// Text annotations created from the PDF toolbar or Command Palette enter a
// placement mode. The SVG icon follows the cursor, canvas-margin clicks do
// nothing, a page click creates the annotation, and Esc cancels.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG, makeBlankPdf } from "./util.ts";
import {
  clientToScreen,
  getClassName,
  getClientRect,
  getFocusedHwnd,
  getRootWindow,
  packCoords,
  postMessage,
  sendMessage,
  sendText,
  setCursorPos,
  sleep,
  VK_DOWN,
  VK_RETURN,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import {
  clickAt,
  findChildByClass,
  findCanvas,
  killAndWait,
  launchControlled,
  pressEscape,
  sendCommand,
  sendCommandSync,
  ngKey,
  ngType,
} from "./win-automation.ts";

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  command: number;
  annotations: number;
  message: string;
  raw: string;
};

async function placementState(client: ControlClient): Promise<PlacementState> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const state = /textPlacement active=(\d+) notification=(\d+) cursor=(\d+) cmd=(\d+) message=(.*)/.exec(raw);
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`text-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    command: +state[4]!,
    annotations: +count[1]!,
    message: state[5]!,
    raw,
  };
}

async function waitForPlacement(client: ControlClient, active: boolean): Promise<PlacementState> {
  const deadline = Date.now() + 5_000;
  let state: PlacementState;
  for (;;) {
    state = await placementState(client);
    if (state.active === active) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`text-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function textToolbarRect(dump: string): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId("CmdCreateAnnotText");
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`text-annotation-placement: Text toolbar button not found\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

// ng's palette is a gpui field, not an Edit. Type through the control pipe.
async function executeFromCommandPaletteNg(client: ControlClient, frame: number): Promise<void> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const openDeadline = Date.now() + 8_000 * SLOW_BUILD_FACTOR;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    raw = String(res[1] ?? "");
    if (res[0] === 0 && raw.startsWith("OK") && raw.includes("editFocus=1")) {
      break;
    }
    if (Date.now() > openDeadline) {
      throw new Error(`text-annotation-placement: command palette did not open\n${raw}`);
    }
    await sleep(50);
  }

  const query = ">Create Text Annotation";
  await ngType(client, query);
  const filterDeadline = Date.now() + 3_000 * SLOW_BUILD_FACTOR;
  let itemCount = 0;
  for (;;) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    raw = String(res[1] ?? "");
    const m = /items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+) cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[2]! === query.length) {
      itemCount = +m[1]!;
      break;
    }
    if (Date.now() > filterDeadline) {
      throw new Error(`text-annotation-placement: palette did not select Text annotation\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotText")) {
      await ngKey(client, VK_RETURN);
      return;
    }
    await ngKey(client, VK_DOWN);
  }
  throw new Error("text-annotation-placement: Text annotation command was not in the filtered palette");
}

async function executeFromCommandPalette(client: ControlClient, frame: number): Promise<void> {
  if (USE_NG) {
    await executeFromCommandPaletteNg(client, frame);
    return;
  }
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const openDeadline = Date.now() + 8_000;
  let palette = 0;
  let edit = 0;
  while (Date.now() < openDeadline) {
    edit = getFocusedHwnd(frame);
    if (edit && getClassName(edit) === "Edit") {
      palette = getRootWindow(edit);
      if (palette && palette !== frame) {
        break;
      }
    }
    await sleep(50);
  }
  if (!palette || !edit) {
    throw new Error("text-annotation-placement: command palette did not open");
  }

  const query = ">Create Text Annotation";
  sendText(edit, query);
  const filterDeadline = Date.now() + 3_000;
  let itemCount = 0;
  for (;;) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+) cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[2]! === query.length) {
      itemCount = +m[1]!;
      break;
    }
    if (Date.now() > filterDeadline) {
      throw new Error(`text-annotation-placement: palette did not select Text annotation\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotText")) {
      postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    await sleep(80);
  }
  throw new Error("text-annotation-placement: Text annotation command was not in the filtered palette");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("text-annotation-placement");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    const canvasRect = getClientRect(canvas);
    const pagePoint = { x: Math.floor(canvasRect.right / 2), y: Math.floor(canvasRect.bottom / 2) };

    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    // The annotation row is visible before gpui writes button bounds.
    const buttonDeadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    let textButton = { x: 0, y: 0, dx: 0, dy: 0 };
    let toolbarDump = "";
    for (;;) {
      toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
      try {
        textButton = textToolbarRect(toolbarDump);
      } catch {
        textButton = { x: 0, y: 0, dx: 0, dy: 0 };
      }
      if (textButton.dx > 0 && textButton.dy > 0) {
        break;
      }
      if (Date.now() > buttonDeadline) {
        throw new Error(`text-annotation-placement: Text toolbar button not laid out\n${toolbarDump}`);
      }
      await sleep(40);
    }
    const bx = textButton.x + Math.floor(textButton.dx / 2);
    const by = textButton.y + Math.floor(textButton.dy / 2);
    if (USE_NG) {
      // The rect is already window dips. clickAt would scale it again.
      const res = await client.request(ControlCommand.TestInput, ["click", bx, by, 0, 0]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0 || !raw.startsWith("OK")) {
        throw new Error(`text-annotation-placement: toolbar click failed: ${raw}`);
      }
    } else {
      const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
      await clickAt(toolbar, bx, by, 0);
    }

    let state = await waitForPlacement(client, true);
    if (
      !state.notification ||
      state.annotations !== 0 ||
      state.message !== "Place text annotation. **Esc** to cancel."
    ) {
      throw new Error(`text-annotation-placement: toolbar did not start clean placement mode\n${state.raw}`);
    }

    const screenPoint = clientToScreen(canvas, pagePoint.x, pagePoint.y);
    setCursorPos(screenPoint.x, screenPoint.y);
    sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(pagePoint.x, pagePoint.y));
    state = await placementState(client);
    if (!state.cursor) {
      throw new Error(`text-annotation-placement: SVG placement cursor was not active\n${state.raw}`);
    }

    await clickAt(canvas, 2, Math.floor(canvasRect.bottom / 2), 0);
    state = await placementState(client);
    if (!state.active || state.annotations !== 0) {
      throw new Error(`text-annotation-placement: click outside the page ended placement\n${state.raw}`);
    }

    await clickAt(canvas, pagePoint.x, pagePoint.y, 0);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 1) {
      throw new Error(`text-annotation-placement: page click did not place exactly one annotation\n${state.raw}`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(pagePoint.x, pagePoint.y));
    state = await placementState(client);
    if (state.active || state.notification || state.annotations !== 2) {
      throw new Error(`text-annotation-placement: a supplied context point did not place immediately\n${state.raw}`);
    }

    await executeFromCommandPalette(client, frame);
    state = await waitForPlacement(client, true);
    if (!state.notification || state.annotations !== 2 || state.command !== cmdId("CmdCreateAnnotText")) {
      throw new Error(`text-annotation-placement: palette did not start placement mode\n${state.raw}`);
    }
    await pressEscape(frame);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 2) {
      throw new Error(`text-annotation-placement: Esc did not cancel without creating\n${state.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("text-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
