// Text annotations created from the PDF toolbar or Command Palette enter a
// placement mode. The SVG icon follows the cursor, canvas-margin clicks do
// nothing, a page click creates the annotation, and Esc cancels.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, assemblePdf } from "./util.ts";
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
  pressEnter,
  pressEscape,
  sendCommand,
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
      await pressEnter(edit);
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

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    const toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const textButton = textToolbarRect(toolbarDump);
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    await clickAt(toolbar, textButton.x + Math.floor(textButton.dx / 2), textButton.y + Math.floor(textButton.dy / 2));

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

    await clickAt(canvas, 2, Math.floor(canvasRect.bottom / 2));
    state = await placementState(client);
    if (!state.active || state.annotations !== 0) {
      throw new Error(`text-annotation-placement: click outside the page ended placement\n${state.raw}`);
    }

    await clickAt(canvas, pagePoint.x, pagePoint.y);
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
