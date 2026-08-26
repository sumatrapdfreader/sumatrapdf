// Rectangle and circle annotations from the PDF toolbar and Command Palette
// can be dragged out or placed with two opposite-corner clicks. Shift
// constrains the preview and annotation to equal width and height.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, assemblePdf } from "./util.ts";
import {
  captureWindowPixels,
  clientToScreen,
  getClassName,
  getClientRect,
  getFocusedHwnd,
  getRootWindow,
  MK_LBUTTON,
  MK_SHIFT,
  packCoords,
  postMessage,
  sendMessage,
  sendText,
  setCursorPos,
  sleep,
  VK_DOWN,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import {
  clickAt,
  findChildByClass,
  findCanvas,
  killAndWait,
  launchControlled,
  pressEnter,
  pressEscape,
  sendCommand,
} from "./win-automation.ts";

type Point = { x: number; y: number };

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  circle: boolean;
  mouseDown: boolean;
  dragged: boolean;
  constrain: boolean;
  command: number;
  page: number;
  preview: { x: number; y: number; dx: number; dy: number };
  annotations: number;
  message: string;
  raw: string;
};

function makeBlankPdf(): string {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  return assemblePdf(objects);
}

async function placementState(client: ControlClient): Promise<PlacementState> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const state =
    /shapePlacement active=(\d+) notification=(\d+) cursor=(\d+) circle=(\d+) mouseDown=(\d+) dragged=(\d+) constrain=(\d+) cmd=(\d+) page=(-?\d+) preview=(-?\d+),(-?\d+),(-?\d+),(-?\d+) message=(.*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`shape-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    circle: state[4] === "1",
    mouseDown: state[5] === "1",
    dragged: state[6] === "1",
    constrain: state[7] === "1",
    command: +state[8]!,
    page: +state[9]!,
    preview: { x: +state[10]!, y: +state[11]!, dx: +state[12]!, dy: +state[13]! },
    annotations: +count[1]!,
    message: state[14]!,
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
      throw new Error(`shape-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function toolbarButtonRect(dump: string, command: string): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId(command);
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`shape-annotation-placement: toolbar button not found for ${command}\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

async function executeFromCommandPalette(client: ControlClient, frame: number, command: string): Promise<void> {
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
    throw new Error("shape-annotation-placement: command palette did not open");
  }

  const query = `>${command === "CmdCreateAnnotCircle" ? "Create Circle Annotation" : "Create Square Annotation"}`;
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
      throw new Error(`shape-annotation-placement: palette query did not settle\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId(command)) {
      await pressEnter(edit);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    await sleep(80);
  }
  throw new Error(`shape-annotation-placement: ${command} was not in the filtered palette`);
}

function moveMouse(canvas: number, point: Point, keys = 0): void {
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, keys, packCoords(point.x, point.y));
}

async function drag(canvas: number, start: Point, end: Point, keys = 0): Promise<void> {
  moveMouse(canvas, start);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON | keys, packCoords(start.x, start.y));
  // SetCapture generates a real-cursor WM_MOUSEMOVE. Let it arrive before the
  // synthetic drag update so it cannot erase the supplied MK_SHIFT state.
  await sleep(50);
  sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON | keys, packCoords(end.x, end.y));
}

function releaseDrag(canvas: number, end: Point, keys = 0): void {
  sendMessage(canvas, WM_LBUTTONUP, keys, packCoords(end.x, end.y));
}

function countPreviewBlue(shot: { w: number; h: number; data: Uint8Array } | null, start: Point, end: Point): number {
  if (!shot) {
    return 0;
  }
  const left = Math.max(0, Math.min(start.x, end.x) - 8);
  const right = Math.min(shot.w - 1, Math.max(start.x, end.x) + 8);
  const top = Math.max(0, Math.min(start.y, end.y) - 8);
  const bottom = Math.min(shot.h - 1, Math.max(start.y, end.y) + 8);
  let count = 0;
  for (let y = top; y <= bottom; y++) {
    for (let x = left; x <= right; x++) {
      const off = (y * shot.w + x) * 4;
      const b = shot.data[off]!;
      const g = shot.data[off + 1]!;
      const r = shot.data[off + 2]!;
      if (b > 150 && b > g + 50 && g > r + 30 && r < 80) {
        count++;
      }
    }
  }
  return count;
}

function shapeScreenRect(raw: string, type: "Square" | "Circle"): { x: number; y: number; dx: number; dy: number } {
  const re = new RegExp(`type=${type} page=1 rect=[^\\n]+ screen=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(raw);
  if (!m) {
    throw new Error(`shape-annotation-placement: ${type} bounds not found\n${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("shape-annotation-placement");
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
    const start = { x: center.x - 65, y: center.y - 60 };
    const rectangleEnd = { x: center.x + 80, y: center.y + 30 };
    const circleEnd = { x: center.x + 70, y: center.y + 10 };
    const outside = { x: 2, y: center.y };

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    const toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const squareButton = toolbarButtonRect(toolbarDump, "CmdCreateAnnotSquare");
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    const clickSquareToolbar = () =>
      clickAt(
        toolbar,
        squareButton.x + Math.floor(squareButton.dx / 2),
        squareButton.y + Math.floor(squareButton.dy / 2),
      );

    await clickSquareToolbar();
    let state = await waitForPlacement(client, true);
    moveMouse(canvas, center);
    state = await placementState(client);
    if (
      !state.notification ||
      !state.cursor ||
      state.circle ||
      state.page !== -1 ||
      state.annotations !== 0 ||
      state.command !== cmdId("CmdCreateAnnotSquare") ||
      state.message !== "Place rectangle annotation. Drag or click twice. **Shift** for a square. **Esc** to cancel."
    ) {
      throw new Error(`shape-annotation-placement: toolbar did not start clean rectangle placement\n${state.raw}`);
    }

    await clickAt(canvas, outside.x, outside.y);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 0) {
      throw new Error(`shape-annotation-placement: outside first click did not cancel cleanly\n${state.raw}`);
    }

    await clickSquareToolbar();
    await waitForPlacement(client, true);
    await clickAt(canvas, start.x, start.y);
    state = await placementState(client);
    if (!state.active || state.mouseDown || state.dragged || state.page !== 1 || state.annotations !== 0) {
      throw new Error(`shape-annotation-placement: first page click did not anchor the rectangle\n${state.raw}`);
    }

    await client.setNotificationsEnabled(false);
    await sleep(100);
    const before = captureWindowPixels(canvas);
    const blueBefore = countPreviewBlue(before, start, rectangleEnd);
    moveMouse(canvas, rectangleEnd);
    await sleep(150);
    let after = captureWindowPixels(canvas);
    for (let i = 0; i < 4; i++) {
      sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(rectangleEnd.x, rectangleEnd.y));
      after = captureWindowPixels(canvas);
      state = await placementState(client);
      if (state.preview.dx >= 120 && state.preview.dy >= 70) {
        break;
      }
    }
    const blueAfter = countPreviewBlue(after, start, rectangleEnd);
    if (blueAfter < blueBefore + 80 || state.preview.dx < 120 || state.preview.dy < 70) {
      throw new Error(
        `shape-annotation-placement: live rectangle preview did not track the mouse (${blueBefore} -> ${blueAfter})\n${state.raw}`,
      );
    }

    await clickAt(canvas, rectangleEnd.x, rectangleEnd.y);
    state = await waitForPlacement(client, false);
    const squareRect = shapeScreenRect(state.raw, "Square");
    if (
      state.annotations !== 1 ||
      squareRect.dx < 120 ||
      squareRect.dy < 70 ||
      Math.abs(squareRect.dx - squareRect.dy) < 30
    ) {
      throw new Error(`shape-annotation-placement: two clicks did not create the requested rectangle\n${state.raw}`);
    }

    await client.setNotificationsEnabled(true);
    await executeFromCommandPalette(client, frame, "CmdCreateAnnotCircle");
    state = await waitForPlacement(client, true);
    if (
      !state.notification ||
      !state.circle ||
      state.annotations !== 1 ||
      state.command !== cmdId("CmdCreateAnnotCircle") ||
      state.message !== "Place circle annotation. Drag or click twice. **Shift** for a circle. **Esc** to cancel."
    ) {
      throw new Error(`shape-annotation-placement: palette did not start clean circle placement\n${state.raw}`);
    }

    await client.setNotificationsEnabled(false);
    await drag(canvas, start, circleEnd, MK_SHIFT);
    await sleep(150);
    let previewShot = captureWindowPixels(canvas);
    for (let i = 0; i < 4; i++) {
      // Windows can queue a physical-cursor move after SetCapture. Re-submit
      // the synthetic Shift move immediately before reading the live preview.
      sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON | MK_SHIFT, packCoords(circleEnd.x, circleEnd.y));
      previewShot = captureWindowPixels(canvas);
      state = await placementState(client);
      if (state.constrain && state.preview.dx === state.preview.dy) {
        break;
      }
    }
    if (
      !state.active ||
      !state.mouseDown ||
      !state.dragged ||
      !state.constrain ||
      state.preview.dx !== state.preview.dy ||
      countPreviewBlue(previewShot, start, { x: start.x + state.preview.dx, y: start.y + state.preview.dy }) < 80
    ) {
      throw new Error(`shape-annotation-placement: Shift-drag did not show a constrained live circle\n${state.raw}`);
    }
    releaseDrag(canvas, circleEnd, MK_SHIFT);
    state = await waitForPlacement(client, false);
    const circleRect = shapeScreenRect(state.raw, "Circle");
    if (state.annotations !== 2 || Math.abs(circleRect.dx - circleRect.dy) > 2 || circleRect.dx < 100) {
      throw new Error(`shape-annotation-placement: Shift-drag did not create a circle\n${state.raw}`);
    }

    await client.setNotificationsEnabled(true);
    sendCommand(frame, cmdId("CmdCreateAnnotSquare"));
    await waitForPlacement(client, true);
    await clickAt(canvas, start.x, start.y);
    await pressEscape(frame);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 2) {
      throw new Error(`shape-annotation-placement: Esc did not cancel without creating\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCreateAnnotCircle"));
    await waitForPlacement(client, true);
    await drag(canvas, start, outside);
    releaseDrag(canvas, outside);
    state = await waitForPlacement(client, false);
    if (state.annotations !== 2) {
      throw new Error(`shape-annotation-placement: off-page drag release created an annotation\n${state.raw}`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotSquare"), packCoords(start.x, start.y));
    state = await placementState(client);
    if (state.active || state.annotations !== 3) {
      throw new Error(`shape-annotation-placement: a supplied context point did not place immediately\n${state.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("shape-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
