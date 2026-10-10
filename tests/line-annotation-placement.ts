// Line annotations created from the PDF toolbar or Command Palette use two
// page clicks. A crosshair and live line preview expose the mode; page-margin
// clicks and Esc cancel without creating an annotation.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG, makeBlankPdf } from "./util.ts";
import {
  captureWindowPixels,
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
  MK_SHIFT,
  VK_DOWN,
  VK_ESCAPE,
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
  pressKey,
  sendCommand,
  ngKey,
  ngType,
  countPreviewBlue,
} from "./win-automation.ts";

type Point = { x: number; y: number };

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  started: boolean;
  command: number;
  page: number;
  annotations: number;
  message: string;
  raw: string;
};

async function placementState(client: ControlClient): Promise<PlacementState> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const state =
    /linePlacement active=(\d+) notification=(\d+) cursor=(\d+) started=(\d+) cmd=(\d+) page=(-?\d+) start=[^ ]+ end=[^ ]+ message=(.*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`line-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    started: state[4] === "1",
    command: +state[5]!,
    page: +state[6]!,
    annotations: +count[1]!,
    message: state[7]!,
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
      throw new Error(`line-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function toolbarButtonRect(dump: string, command: string): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId(command);
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`line-annotation-placement: toolbar button not found for ${command}\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

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
      throw new Error(`line-annotation-placement: command palette did not open\n${raw}`);
    }
    await sleep(50);
  }

  const query = ">Create Line Annotation";
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
      throw new Error(`line-annotation-placement: palette query did not settle\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotLine")) {
      await ngKey(client, VK_RETURN);
      return;
    }
    await ngKey(client, VK_DOWN);
  }
  throw new Error("line-annotation-placement: Line annotation command was not in the filtered palette");
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
    throw new Error("line-annotation-placement: command palette did not open");
  }

  const query = ">Create Line Annotation";
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
      throw new Error(`line-annotation-placement: palette query did not settle\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotLine")) {
      await pressKey(edit, VK_RETURN, 0);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
  }
  throw new Error("line-annotation-placement: Line annotation command was not in the filtered palette");
}

function moveMouse(canvas: number, point: Point, key = 0): void {
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, key, packCoords(point.x, point.y));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("line-annotation-placement");
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
    const center = { x: Math.floor(canvasRect.right / 2), y: Math.floor(canvasRect.bottom / 2) };
    const start = { x: center.x - 90, y: center.y - 70 };
    const end = { x: center.x + 100, y: center.y + 80 };
    const outside = { x: 2, y: center.y };

    sendMessage(frame, WM_COMMAND, cmdId("CmdToggleEditPDF"), 0);
    const buttonDeadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    let lineButton = { x: 0, y: 0, dx: 0, dy: 0 };
    let toolbarDump = "";
    for (;;) {
      toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
      try {
        lineButton = toolbarButtonRect(toolbarDump, "CmdCreateAnnotLine");
      } catch {
        lineButton = { x: 0, y: 0, dx: 0, dy: 0 };
      }
      if (lineButton.dx > 0 && lineButton.dy > 0) {
        break;
      }
      if (Date.now() > buttonDeadline) {
        throw new Error(`line-annotation-placement: toolbar button not laid out\n${toolbarDump}`);
      }
      await sleep(40);
    }
    const bx = lineButton.x + Math.floor(lineButton.dx / 2);
    const by = lineButton.y + Math.floor(lineButton.dy / 2);
    const clickLineToolbar = async () => {
      if (USE_NG) {
        const res = await client.request(ControlCommand.TestInput, ["click", bx, by, 0, 0]);
        const raw = String(res[1] ?? "");
        if (res[0] !== 0 || !raw.startsWith("OK")) {
          throw new Error(`line-annotation-placement: toolbar click failed: ${raw}`);
        }
        return;
      }
      const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
      await clickAt(toolbar, bx, by, 0);
    };

    await clickLineToolbar();
    let state = await waitForPlacement(client, true);
    moveMouse(canvas, center);
    state = await placementState(client);
    if (
      !state.notification ||
      !state.cursor ||
      state.started ||
      state.annotations !== 0 ||
      state.command !== cmdId("CmdCreateAnnotLine") ||
      state.message !== "Place line annotation. **Shift** to snap to multiples of 45 degrees. **Esc** to cancel."
    ) {
      throw new Error(`line-annotation-placement: toolbar did not start clean placement mode\n${state.raw}`);
    }

    await clickAt(canvas, outside.x, outside.y, 0);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 0) {
      throw new Error(`line-annotation-placement: outside first click did not cancel cleanly\n${state.raw}`);
    }

    await clickLineToolbar();
    await waitForPlacement(client, true);
    await clickAt(canvas, start.x, start.y, 0);
    state = await placementState(client);
    if (!state.active || !state.started || state.page !== 1 || state.annotations !== 0) {
      throw new Error(`line-annotation-placement: first page click did not anchor the line\n${state.raw}`);
    }

    await client.setNotificationsEnabled(false);
    const before = IS_MAC ? null : captureWindowPixels(canvas);
    const blueBefore = countPreviewBlue(before, start, end);
    // SetCursorPos queues a WM_MOUSEMOVE with no Shift, which can land after the
    // synthetic one and undo the snap. Resend until the 45-degree preview sticks.
    moveMouse(canvas, end, MK_SHIFT);
    let snapped: RegExpExecArray | null = null;
    for (let i = 0; i < 4; i++) {
      sendMessage(canvas, WM_MOUSEMOVE, MK_SHIFT, packCoords(end.x, end.y));
      state = await placementState(client);
      snapped = /end=(-?\d+),(-?\d+)/.exec(state.raw);
      if (snapped && Math.abs(+snapped[1]! - start.x - (+snapped[2]! - start.y)) <= 2) {
        break;
      }
      snapped = null;
    }
    if (!snapped) {
      throw new Error(`line-annotation-placement: Shift did not snap preview to 45 degrees\n${state.raw}`);
    }

    moveMouse(canvas, end);
    state = await placementState(client);
    if (!snapped || !state.raw.includes(`end=${end.x},${end.y}`)) {
      throw new Error(`line-annotation-placement: releasing Shift did not restore the pointer endpoint\n${state.raw}`);
    }
    if (IS_MAC) {
      console.log("SKIP line-annotation-placement: preview pixels are read from a window DC");
    } else {
      await sleep(150);
      const after = captureWindowPixels(canvas);
      const blueAfter = countPreviewBlue(after, start, end);
      if (blueAfter < blueBefore + 80) {
        throw new Error(
          `line-annotation-placement: live preview did not paint while moving (${blueBefore} -> ${blueAfter})`,
        );
      }
    }

    await clickAt(canvas, outside.x, outside.y, 0);
    state = await waitForPlacement(client, false);
    if (state.annotations !== 0) {
      throw new Error(`line-annotation-placement: outside second click created an annotation\n${state.raw}`);
    }

    await client.setNotificationsEnabled(true);
    await executeFromCommandPalette(client, frame);
    state = await waitForPlacement(client, true);
    if (!state.notification || state.started || state.annotations !== 0) {
      throw new Error(`line-annotation-placement: palette did not start placement mode\n${state.raw}`);
    }
    await clickAt(canvas, start.x, start.y, 0);
    moveMouse(canvas, end, MK_SHIFT);
    await clickAt(canvas, end.x, end.y, 0, MK_SHIFT);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 1) {
      throw new Error(`line-annotation-placement: second page click did not create exactly one line\n${state.raw}`);
    }

    await executeFromCommandPalette(client, frame);
    await waitForPlacement(client, true);
    await clickAt(canvas, start.x, start.y, 0);
    await pressKey(frame, VK_ESCAPE, 0);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 1) {
      throw new Error(`line-annotation-placement: Esc did not cancel without creating\n${state.raw}`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotLine"), packCoords(start.x, start.y));
    state = await placementState(client);
    if (state.active || state.annotations !== 2) {
      throw new Error(`line-annotation-placement: a supplied context point did not place immediately\n${state.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("line-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
