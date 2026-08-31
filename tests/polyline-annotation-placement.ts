// Polyline annotations from the PDF toolbar and Command Palette collect page
// vertices with a live preview. Double-click, right-click, Enter, and Space
// finish the path; off-page clicks and Esc cancel it.

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
  MK_CONTROL,
  MK_LBUTTON,
  MK_RBUTTON,
  packCoords,
  postMessage,
  sendMessage,
  sendText,
  setCursorPos,
  sleep,
  VK_DOWN,
  VK_SPACE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
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
  points: number;
  command: number;
  page: number;
  annotations: number;
  message: string;
  raw: string;
};

const notification =
  "Place polyline annotation. **Double-click**, **right-click**, **Space**, or **Enter** to finish, " +
  "**Ctrl+click** to close it. **Esc** to cancel.";

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
    /polyLinePlacement active=(\d+) notification=(\d+) cursor=(\d+) points=(\d+) cmd=(\d+) page=(-?\d+) end=[^ ]+ message=(.*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`polyline-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    points: +state[4]!,
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
      throw new Error(`polyline-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function toolbarButtonRect(dump: string): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId("CmdCreateAnnotPolyLine");
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`polyline-annotation-placement: toolbar button not found\n${dump}`);
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
    throw new Error("polyline-annotation-placement: command palette did not open");
  }

  const query = ">Create Polyline Annotation";
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
      throw new Error(`polyline-annotation-placement: palette query did not settle\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotPolyLine")) {
      await pressEnter(edit);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    await sleep(80);
  }
  throw new Error("polyline-annotation-placement: Polyline command was not in the filtered palette");
}

function moveMouse(canvas: number, point: Point): void {
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(point.x, point.y));
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

async function clickPoints(canvas: number, points: Point[]): Promise<void> {
  for (const point of points) {
    await clickAt(canvas, point.x, point.y, 80);
  }
}

async function expectFinished(client: ControlClient, annotations: number, gesture: string): Promise<PlacementState> {
  const state = await waitForPlacement(client, false);
  if (state.notification || state.annotations !== annotations || !state.raw.includes("type=PolyLine page=1")) {
    throw new Error(`polyline-annotation-placement: ${gesture} did not finish cleanly\n${state.raw}`);
  }
  return state;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("polyline-annotation-placement");
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
    const p1 = { x: center.x - 120, y: center.y - 90 };
    const p2 = { x: center.x + 105, y: center.y - 25 };
    const p3 = { x: center.x - 75, y: center.y + 105 };
    const p4 = { x: center.x + 60, y: center.y + 60 };
    const outside = { x: 2, y: center.y };

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    const toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const button = toolbarButtonRect(toolbarDump);
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    const clickToolbar = () =>
      clickAt(toolbar, button.x + Math.floor(button.dx / 2), button.y + Math.floor(button.dy / 2));

    await clickToolbar();
    let state = await waitForPlacement(client, true);
    moveMouse(canvas, center);
    state = await placementState(client);
    if (
      !state.notification ||
      !state.cursor ||
      state.points !== 0 ||
      state.annotations !== 0 ||
      state.command !== cmdId("CmdCreateAnnotPolyLine") ||
      state.message !== notification
    ) {
      throw new Error(`polyline-annotation-placement: toolbar did not start clean placement mode\n${state.raw}`);
    }

    await clickAt(canvas, outside.x, outside.y);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 0) {
      throw new Error(`polyline-annotation-placement: outside first click did not cancel cleanly\n${state.raw}`);
    }

    await clickToolbar();
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y);
    state = await placementState(client);
    if (!state.active || state.points !== 1 || state.page !== 1 || state.annotations !== 0) {
      throw new Error(`polyline-annotation-placement: first page click did not anchor the path\n${state.raw}`);
    }
    await client.setNotificationsEnabled(false);
    await sleep(100);
    const before = captureWindowPixels(canvas);
    const blueBefore = countPreviewBlue(before, p1, p2);
    moveMouse(canvas, p2);
    await sleep(150);
    const after = captureWindowPixels(canvas);
    const blueAfter = countPreviewBlue(after, p1, p2);
    if (blueAfter < blueBefore + 80) {
      throw new Error(`polyline-annotation-placement: live preview did not paint (${blueBefore} -> ${blueAfter})`);
    }
    await clickPoints(canvas, [p2, p3]);
    state = await placementState(client);
    if (!state.active || state.points !== 3 || state.annotations !== 0) {
      throw new Error(`polyline-annotation-placement: clicks did not accumulate vertices\n${state.raw}`);
    }
    const rightLp = packCoords(p3.x, p3.y);
    sendMessage(canvas, WM_RBUTTONDOWN, MK_RBUTTON, rightLp);
    sendMessage(canvas, WM_RBUTTONUP, 0, rightLp);
    state = await expectFinished(client, 1, "right-click");
    const bounds = /type=PolyLine page=1 rect=[^\n]+ screen=-?\d+,-?\d+,(-?\d+),(-?\d+)/.exec(state.raw);
    if (!bounds || +bounds[1]! < 180 || +bounds[2]! < 150) {
      throw new Error(
        `polyline-annotation-placement: placed vertices did not define the annotation bounds\n${state.raw}`,
      );
    }

    await client.setNotificationsEnabled(true);
    await executeFromCommandPalette(client, frame);
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2, p3]);
    const dblLp = packCoords(p3.x, p3.y);
    sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, dblLp);
    sendMessage(canvas, WM_LBUTTONUP, 0, dblLp);
    await expectFinished(client, 2, "double-click");

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y, 80);
    await pressEnter(frame);
    state = await placementState(client);
    if (!state.active || state.points !== 1 || state.annotations !== 2) {
      throw new Error(`polyline-annotation-placement: Enter finished an invalid one-point path\n${state.raw}`);
    }
    await clickAt(canvas, p2.x, p2.y, 80);
    await pressEnter(frame);
    await expectFinished(client, 3, "Enter");

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2, p3]);
    await pressKey(frame, VK_SPACE);
    await expectFinished(client, 4, "Space");

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2]);
    await pressEscape(frame);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 4) {
      throw new Error(`polyline-annotation-placement: Esc did not cancel without creating\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCreateAnnotPolyLine"));
    await waitForPlacement(client, true);
    await clickPoints(canvas, [p1, p2]);
    await clickAt(canvas, outside.x, outside.y);
    state = await waitForPlacement(client, false);
    if (state.annotations !== 4) {
      throw new Error(`polyline-annotation-placement: outside later click created an annotation\n${state.raw}`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotPolyLine"), packCoords(p1.x, p1.y));
    state = await placementState(client);
    if (state.active || state.annotations !== 5) {
      throw new Error(
        `polyline-annotation-placement: a supplied context point did not place immediately\n${state.raw}`,
      );
    }

    // Ctrl+click commits the vertex and closes the path back to the first
    // point, so the annotation repeats it (issue #6119)
    await executeFromCommandPalette(client, frame);
    await waitForPlacement(client, true);
    // enough vertices that closing the path grows the point vec: appending an
    // element of the vec to itself used to read the freed buffer
    await clickPoints(canvas, [p1, p2, p3]);
    await clickAt(canvas, p4.x, p4.y, 350, MK_CONTROL);
    state = await expectFinished(client, 6, "Ctrl+click");
    // the dump lists every annotation; the one just placed is the last
    const all = [...state.raw.matchAll(/polyline vertices=(\d+) closed=(\d)/g)];
    const closed = all[all.length - 1];
    if (!closed || closed[2] !== "1") {
      throw new Error(`polyline-annotation-placement: Ctrl+click did not close the path\n${state.raw}`);
    }
    if (+closed[1]! !== 5) {
      throw new Error(
        `polyline-annotation-placement: closed path has ${closed[1]} vertices, wanted the 4 clicked plus the repeat`,
      );
    }

    // two vertices are a single segment: closing would double back on it, so
    // Ctrl+click keeps collecting instead
    await executeFromCommandPalette(client, frame);
    await waitForPlacement(client, true);
    await clickAt(canvas, p1.x, p1.y);
    await clickAt(canvas, p2.x, p2.y, 350, MK_CONTROL);
    state = await placementState(client);
    if (!state.active || state.points !== 2) {
      throw new Error(`polyline-annotation-placement: Ctrl+click closed a single segment\n${state.raw}`);
    }
    await pressEscape(frame);
    await waitForPlacement(client, false);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("polyline-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
