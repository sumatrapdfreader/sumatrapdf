// Ink annotations from the PDF toolbar and Command Palette collect one or
// more freehand strokes with a live preview. Enter commits the combined
// InkList; Esc discards it.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, assemblePdf, SLOW_BUILD_FACTOR } from "./util.ts";
import {
  captureWindowPixels,
  clientToScreen,
  getClassName,
  getClientRect,
  getFocusedHwnd,
  getRootWindow,
  MK_LBUTTON,
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
  pressKey,
  sendCommand,
} from "./win-automation.ts";

type Point = { x: number; y: number };

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  mouseDown: boolean;
  strokes: number;
  points: number;
  command: number;
  page: number;
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
    /inkPlacement active=(\d+) notification=(\d+) cursor=(\d+) mouseDown=(\d+) strokes=(\d+) points=(\d+) cmd=(\d+) page=(-?\d+) message=(.*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`ink-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    mouseDown: state[4] === "1",
    strokes: +state[5]!,
    points: +state[6]!,
    command: +state[7]!,
    page: +state[8]!,
    annotations: +count[1]!,
    message: state[9]!,
    raw,
  };
}

async function waitForPlacement(client: ControlClient, active: boolean): Promise<PlacementState> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let state: PlacementState;
  for (;;) {
    state = await placementState(client);
    if (state.active === active) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`ink-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function toolbarButtonRect(dump: string): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId("CmdCreateAnnotInk");
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`ink-annotation-placement: Ink toolbar button not found\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

async function executeFromCommandPalette(client: ControlClient, frame: number): Promise<void> {
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
    throw new Error("ink-annotation-placement: command palette did not open");
  }

  const query = ">Create Ink Annotation";
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
      throw new Error(`ink-annotation-placement: palette query did not settle\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotInk")) {
      await pressEnter(edit);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    await sleep(80);
  }
  throw new Error("ink-annotation-placement: Ink command was not in the filtered palette");
}

function moveMouse(canvas: number, point: Point): void {
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(point.x, point.y));
}

async function drawStroke(canvas: number, points: Point[]): Promise<void> {
  const first = points[0]!;
  moveMouse(canvas, first);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(first.x, first.y));
  // Let SetCapture's physical-cursor move settle before submitting the path.
  await sleep(50);
  for (let i = 1; i < points.length; i++) {
    const point = points[i]!;
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(point.x, point.y));
  }
  const last = points[points.length - 1]!;
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(last.x, last.y));
  await sleep(100);
}

function countPreviewBlue(shot: { w: number; h: number; data: Uint8Array } | null, points: Point[]): number {
  if (!shot) {
    return 0;
  }
  let left = points[0]!.x;
  let right = left;
  let top = points[0]!.y;
  let bottom = top;
  for (const point of points) {
    left = Math.min(left, point.x);
    right = Math.max(right, point.x);
    top = Math.min(top, point.y);
    bottom = Math.max(bottom, point.y);
  }
  left = Math.max(0, left - 8);
  right = Math.min(shot.w - 1, right + 8);
  top = Math.max(0, top - 8);
  bottom = Math.min(shot.h - 1, bottom + 8);
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

function inkScreenRects(raw: string): { x: number; y: number; dx: number; dy: number }[] {
  return Array.from(raw.matchAll(/type=Ink page=1 rect=[^\n]+ screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g), (m) => ({
    x: +m[1]!,
    y: +m[2]!,
    dx: +m[3]!,
    dy: +m[4]!,
  }));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ink-annotation-placement");
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
    const stroke1 = [
      { x: center.x - 105, y: center.y - 75 },
      { x: center.x - 80, y: center.y - 30 },
      { x: center.x - 55, y: center.y - 65 },
      { x: center.x - 25, y: center.y - 20 },
    ];
    const stroke2 = [
      { x: center.x + 15, y: center.y + 15 },
      { x: center.x + 45, y: center.y + 65 },
      { x: center.x + 75, y: center.y + 25 },
      { x: center.x + 110, y: center.y + 75 },
    ];
    const outside = { x: 2, y: center.y };

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    const toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const button = toolbarButtonRect(toolbarDump);
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    const clickInkToolbar = () =>
      clickAt(toolbar, button.x + Math.floor(button.dx / 2), button.y + Math.floor(button.dy / 2));

    await clickInkToolbar();
    let state = await waitForPlacement(client, true);
    moveMouse(canvas, center);
    state = await placementState(client);
    if (
      !state.notification ||
      !state.cursor ||
      state.mouseDown ||
      state.strokes !== 0 ||
      state.points !== 0 ||
      state.annotations !== 0 ||
      state.command !== cmdId("CmdCreateAnnotInk") ||
      state.message !== "Draw ink annotation. **Enter** to finish. **Esc** to cancel."
    ) {
      throw new Error(`ink-annotation-placement: toolbar did not start clean placement mode\n${state.raw}`);
    }

    await clickAt(canvas, outside.x, outside.y);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 0) {
      throw new Error(`ink-annotation-placement: outside first click did not cancel cleanly\n${state.raw}`);
    }

    await executeFromCommandPalette(client, frame);
    state = await waitForPlacement(client, true);
    moveMouse(canvas, center);
    state = await placementState(client);
    if (!state.notification || !state.cursor || state.annotations !== 0) {
      throw new Error(`ink-annotation-placement: palette did not start placement mode\n${state.raw}`);
    }

    await client.setNotificationsEnabled(false);
    const before = captureWindowPixels(canvas);
    const blueBefore = countPreviewBlue(before, stroke1);
    await drawStroke(canvas, stroke1);
    const after = captureWindowPixels(canvas);
    const blueAfter = countPreviewBlue(after, stroke1);
    state = await placementState(client);
    if (
      !state.active ||
      state.mouseDown ||
      state.strokes !== 1 ||
      state.points < stroke1.length ||
      state.page !== 1 ||
      state.annotations !== 0 ||
      blueAfter < blueBefore + 40
    ) {
      throw new Error(
        `ink-annotation-placement: first stroke did not remain live (${blueBefore} -> ${blueAfter})\n${state.raw}`,
      );
    }

    await drawStroke(canvas, stroke2);
    state = await placementState(client);
    if (
      !state.active ||
      state.strokes !== 2 ||
      state.points < stroke1.length + stroke2.length ||
      state.annotations !== 0
    ) {
      throw new Error(`ink-annotation-placement: second stroke was not added to the same markup\n${state.raw}`);
    }

    await pressKey(frame, VK_RETURN, 400 * SLOW_BUILD_FACTOR);
    state = await waitForPlacement(client, false);
    const firstInk = inkScreenRects(state.raw)[0];
    if (!firstInk || state.notification || state.annotations !== 1 || firstInk.dx < 180 || firstInk.dy < 120) {
      throw new Error(`ink-annotation-placement: Enter did not create the combined Ink annotation\n${state.raw}`);
    }

    await client.setNotificationsEnabled(true);
    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await waitForPlacement(client, true);
    await client.setNotificationsEnabled(false);
    await drawStroke(canvas, stroke1);
    await pressEscape(frame);
    state = await waitForPlacement(client, false);
    if (state.annotations !== 1) {
      throw new Error(`ink-annotation-placement: Esc did not discard the in-progress Ink annotation\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await waitForPlacement(client, true);
    await drawStroke(canvas, stroke2);
    sendCommand(frame, cmdId("CmdCreateAnnotLine"));
    state = await waitForPlacement(client, false);
    if (state.annotations !== 2 || !state.raw.includes("linePlacement active=1")) {
      throw new Error(`ink-annotation-placement: switching tools did not commit completed strokes\n${state.raw}`);
    }
    await pressEscape(frame);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotInk"), packCoords(stroke1[0]!.x, stroke1[0]!.y));
    state = await placementState(client);
    if (state.active || state.annotations !== 3) {
      throw new Error(`ink-annotation-placement: a supplied context point did not place immediately\n${state.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("ink-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
