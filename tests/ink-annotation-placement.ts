// Ink annotations from the PDF toolbar and Command Palette: each stroke
// commits on release and the tool stays on. Esc or an empty click leaves it.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, pollUntil, runStandalone, tmpPath, assemblePdf, SLOW_BUILD_FACTOR, USE_NG } from "./util.ts";
import {
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
  VK_ESCAPE,
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
  pressEscape,
  sendCommand,
  sendCommandSync,
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

// null while the tab has no engine yet. The caller waits that out.
async function placementState(client: ControlClient): Promise<PlacementState | null> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const state =
    /inkPlacement active=(\d+) notification=(\d+) cursor=(\d+) mouseDown=(\d+) strokes=(\d+) points=(\d+) cmd=(\d+) page=(-?\d+) message=(.*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !count || !state) {
    if (raw.includes("NOTREADY")) {
      return null;
    }
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

async function waitForPlacement(client: ControlClient, active: boolean, step: string): Promise<PlacementState> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let state: PlacementState | null = null;
  for (;;) {
    state = await placementState(client);
    if (state && state.active === active) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(
        `ink-annotation-placement: ${step}: active did not become ${active}\n${state?.raw ?? "NOTREADY"}`,
      );
    }
    await sleep(40);
  }
}

function toolbarButtonRect(dump: string): { x: number; y: number; dx: number; dy: number } | null {
  const id = cmdId("CmdCreateAnnotInk");
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    return null;
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

async function ngType(client: ControlClient, text: string): Promise<void> {
  for (const ch of text) {
    const res = await client.request(ControlCommand.TestInput, ["char", ch.codePointAt(0)!]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0 || !raw.startsWith("OK")) {
      throw new Error(`ink-annotation-placement: palette type failed: ${raw}`);
    }
  }
}

async function ngKey(client: ControlClient, vk: number): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, ["key", vk, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`ink-annotation-placement: palette key failed: ${raw}`);
  }
}

// ng's palette is a gpui field, not an Edit. Type through the control pipe.
async function executeFromCommandPaletteNg(client: ControlClient, frame: number): Promise<void> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const openDeadline = Date.now() + 8_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    raw = String(res[1] ?? "");
    if (res[0] === 0 && raw.startsWith("OK") && raw.includes("editFocus=1")) {
      break;
    }
    if (Date.now() > openDeadline) {
      throw new Error(`ink-annotation-placement: command palette did not open\n${raw}`);
    }
    await sleep(50);
  }

  const query = ">Create Ink Annotation";
  await ngType(client, query);
  const filterDeadline = Date.now() + 3_000;
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
      throw new Error(`ink-annotation-placement: palette query did not settle\n${raw}`);
    }
    await sleep(40);
  }

  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === cmdId("CmdCreateAnnotInk")) {
      await ngKey(client, VK_RETURN);
      return;
    }
    await ngKey(client, VK_DOWN);
  }
  throw new Error("ink-annotation-placement: Ink command was not in the filtered palette");
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
      postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    await sleep(80);
  }
  throw new Error("ink-annotation-placement: Ink command was not in the filtered palette");
}

async function ngMouse(client: ControlClient, kind: string, point: Point, button: number): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, [kind, point.x, point.y, button, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`ink-annotation-placement: ${kind} failed: ${raw}`);
  }
}

async function moveMouse(client: ControlClient, canvas: number, point: Point): Promise<void> {
  // ng: a posted move is hit-tested at the real cursor, which a locked
  // desktop keeps at 0,0, so the preview never follows the point.
  if (USE_NG) {
    await ngMouse(client, "move", point, 0);
    return;
  }
  const screen = clientToScreen(canvas, point.x, point.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(point.x, point.y));
}

async function drawStroke(client: ControlClient, canvas: number, points: Point[]): Promise<void> {
  if (USE_NG) {
    const first = points[0]!;
    await ngMouse(client, "down", first, 0);
    for (let i = 1; i < points.length; i++) {
      await ngMouse(client, "move", points[i]!, 1);
    }
    await ngMouse(client, "up", points[points.length - 1]!, 0);
    await sleep(100);
    return;
  }
  const first = points[0]!;
  await moveMouse(client, canvas, first);
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

    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    // The annotation row reports a 0,0 rect until it lays out. A click there
    // is the frame origin and drops the document.
    const toolbarDump = await pollUntil(
      async () => String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? ""),
      (dump) => {
        const b = toolbarButtonRect(dump);
        return !!b && b.dx > 0 && b.dy > 0;
      },
      { error: "ink-annotation-placement: Ink toolbar button not found" },
    );
    const button = toolbarButtonRect(toolbarDump)!;
    const clickInkToolbar = async () => {
      const x = button.x + Math.floor(button.dx / 2);
      const y = button.y + Math.floor(button.dy / 2);
      // ng draws the toolbar in the frame. A posted down/up pair can be split
      // by a cursor snap, which gpui treats as a drag.
      if (USE_NG) {
        await ngMouse(client, "click", { x, y }, 0);
        return;
      }
      const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
      await clickAt(toolbar, x, y, 0);
    };

    await clickInkToolbar();
    let state = await waitForPlacement(client, true, "toolbar");
    await moveMouse(client, canvas, center);
    state = await placementState(client);
    if (
      !state ||
      !state.notification ||
      !state.cursor ||
      state.mouseDown ||
      state.strokes !== 0 ||
      state.points !== 0 ||
      state.annotations !== 0 ||
      state.command !== cmdId("CmdCreateAnnotInk") ||
      state.message !== "Draw ink annotation. Release to finish. **Esc** to cancel."
    ) {
      throw new Error(
        `ink-annotation-placement: toolbar did not start clean placement mode\n${state?.raw ?? "NOTREADY"}`,
      );
    }

    if (USE_NG) {
      await ngMouse(client, "click", outside, 0);
    } else {
      await clickAt(canvas, outside.x, outside.y, 0);
    }
    state = await waitForPlacement(client, false, "outside click");
    if (state.notification || state.annotations !== 0) {
      throw new Error(`ink-annotation-placement: outside first click did not cancel cleanly\n${state.raw}`);
    }

    await executeFromCommandPalette(client, frame);
    state = await waitForPlacement(client, true, "palette");
    await moveMouse(client, canvas, center);
    state = await placementState(client);
    if (!state || !state.notification || !state.cursor || state.annotations !== 0) {
      throw new Error(`ink-annotation-placement: palette did not start placement mode\n${state?.raw ?? "NOTREADY"}`);
    }

    await client.setNotificationsEnabled(false);
    await drawStroke(client, canvas, stroke1);
    state = await waitForPlacement(client, true, "first stroke");
    if (state.mouseDown || state.strokes !== 0 || state.annotations !== 1) {
      throw new Error(`ink-annotation-placement: first stroke did not commit on release\n${state.raw}`);
    }

    await drawStroke(client, canvas, stroke2);
    state = await placementState(client);
    if (!state || !state.active || state.annotations !== 2) {
      throw new Error(
        `ink-annotation-placement: second stroke did not commit as its own ink\n${state?.raw ?? "NOTREADY"}`,
      );
    }

    postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    state = await waitForPlacement(client, false, "esc after strokes");
    if (state.annotations !== 2) {
      throw new Error(`ink-annotation-placement: Esc dropped committed ink\n${state.raw}`);
    }

    await client.setNotificationsEnabled(true);
    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await waitForPlacement(client, true, "command");
    postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    state = await waitForPlacement(client, false, "esc empty tool");
    if (state.annotations !== 2) {
      throw new Error(`ink-annotation-placement: Esc on an empty tool created or deleted ink\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await waitForPlacement(client, true, "command before line");
    await client.setNotificationsEnabled(false);
    await drawStroke(client, canvas, stroke1);
    sendCommand(frame, cmdId("CmdCreateAnnotLine"));
    state = await waitForPlacement(client, false, "switch to line");
    if (state.annotations !== 3 || !state.raw.includes("linePlacement active=1")) {
      throw new Error(`ink-annotation-placement: switching tools lost the last stroke\n${state.raw}`);
    }
    await pressEscape(frame);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotInk"), packCoords(stroke1[0]!.x, stroke1[0]!.y));
    state = await placementState(client);
    if (!state || state.active || state.annotations !== 4) {
      throw new Error(
        `ink-annotation-placement: a supplied context point did not place immediately\n${state?.raw ?? "NOTREADY"}`,
      );
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
