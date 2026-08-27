// Free text annotations from the PDF toolbar or Command Palette enter a
// placement mode: a preview box the size of the annotation follows the cursor,
// canvas-margin clicks do nothing, a page click creates it exactly where the
// preview was, and Esc cancels.

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

type Rect = { x: number; y: number; dx: number; dy: number };

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  preview: boolean;
  command: number;
  annotations: number;
  message: string;
  previewRect: Rect;
  raw: string;
};

const kExpectedMessage = "Place free text annotation. **Esc** to cancel.";

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
    /freeTextPlacement active=(\d+) notification=(\d+) cursor=(\d+) preview=(\d+) cmd=(-?\d+) message=(.*)/.exec(raw);
  const rect = /freeTextPreview rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !count || !state || !rect) {
    throw new Error(`free-text-annotation-placement: could not read state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    preview: state[4] === "1",
    command: +state[5]!,
    annotations: +count[1]!,
    message: state[6]!.trim(),
    previewRect: { x: +rect[1]!, y: +rect[2]!, dx: +rect[3]!, dy: +rect[4]! },
    raw,
  };
}

async function waitForPlacement(client: ControlClient, active: boolean): Promise<PlacementState> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const state = await placementState(client);
    if (state.active === active) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`free-text-annotation-placement: active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

// screen rect of the currently selected annotation
async function selectedAnnotRect(client: ControlClient): Promise<Rect> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`free-text-annotation-placement: could not read selected annotation: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function toolbarRect(dump: string): Rect {
  const id = cmdId("CmdCreateAnnotFreeText");
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`free-text-annotation-placement: Free Text toolbar button not found\n${dump}`);
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
    throw new Error("free-text-annotation-placement: command palette did not open");
  }

  const query = ">Create Free Text Annotation";
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
      throw new Error(`free-text-annotation-placement: palette did not filter\n${raw}`);
    }
    await sleep(40);
  }

  const want = cmdId("CmdCreateAnnotFreeText");
  for (let i = 0; i < itemCount; i++) {
    const res = await client.request(ControlCommand.TestCommandPalette, []);
    const raw = String(res[1] ?? "");
    const m = /cmd=(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[1]! === want) {
      await pressEnter(edit);
      return;
    }
    postMessage(edit, WM_KEYDOWN, VK_DOWN, 0);
    await sleep(80);
  }
  throw new Error("free-text-annotation-placement: command was not in the filtered palette");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("free-text-annotation-placement");
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
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    const canvasRect = getClientRect(canvas);
    const pagePoint = { x: Math.floor(canvasRect.right / 2), y: Math.floor(canvasRect.bottom / 2) };
    const wantCmd = cmdId("CmdCreateAnnotFreeText");

    const toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const button = toolbarRect(toolbarDump);
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    await clickAt(toolbar, button.x + Math.floor(button.dx / 2), button.y + Math.floor(button.dy / 2));

    let state = await waitForPlacement(client, true);
    if (!state.notification || state.annotations !== 0 || state.message !== kExpectedMessage) {
      throw new Error(`free-text-annotation-placement: toolbar did not start placement\n${state.raw}`);
    }

    const screenPoint = clientToScreen(canvas, pagePoint.x, pagePoint.y);
    setCursorPos(screenPoint.x, screenPoint.y);
    sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(pagePoint.x, pagePoint.y));
    state = await placementState(client);
    if (!state.cursor || !state.preview) {
      throw new Error(`free-text-annotation-placement: preview/cursor was not active\n${state.raw}`);
    }
    const preview = state.previewRect;
    // sized to one line of the placeholder text, not MuPDF's 200x100 default
    if (preview.dx < 20 || preview.dy < 8 || preview.dy > preview.dx / 2) {
      throw new Error(`free-text-annotation-placement: preview box is not text sized: ${JSON.stringify(preview)}`);
    }
    if (preview.x !== pagePoint.x || preview.y !== pagePoint.y) {
      throw new Error(
        `free-text-annotation-placement: preview not at the cursor ${JSON.stringify(pagePoint)}: ${JSON.stringify(preview)}`,
      );
    }

    await clickAt(canvas, 2, Math.floor(canvasRect.bottom / 2));
    state = await placementState(client);
    if (!state.active || state.annotations !== 0) {
      throw new Error(`free-text-annotation-placement: click outside the page ended placement\n${state.raw}`);
    }

    setCursorPos(screenPoint.x, screenPoint.y);
    sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(pagePoint.x, pagePoint.y));
    await clickAt(canvas, pagePoint.x, pagePoint.y);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 1) {
      throw new Error(`free-text-annotation-placement: page click did not place one annotation\n${state.raw}`);
    }

    // the annotation must land where the preview box was, at the same size
    const created = await selectedAnnotRect(client);
    const slack = 4;
    const off = [created.x - preview.x, created.y - preview.y, created.dx - preview.dx, created.dy - preview.dy];
    if (off.some((v) => Math.abs(v) > slack)) {
      throw new Error(
        `free-text-annotation-placement: created ${JSON.stringify(created)} != preview ${JSON.stringify(preview)}`,
      );
    }

    // the context-menu path passes a point and still creates immediately
    sendMessage(frame, WM_COMMAND, wantCmd, packCoords(pagePoint.x + 40, pagePoint.y + 40));
    state = await placementState(client);
    if (state.active || state.notification || state.annotations !== 2) {
      throw new Error(`free-text-annotation-placement: context point did not place immediately\n${state.raw}`);
    }

    await executeFromCommandPalette(client, frame);
    state = await waitForPlacement(client, true);
    if (!state.notification || state.annotations !== 2 || state.command !== wantCmd) {
      throw new Error(`free-text-annotation-placement: palette did not start placement\n${state.raw}`);
    }
    await pressEscape(frame);
    state = await waitForPlacement(client, false);
    if (state.notification || state.annotations !== 2) {
      throw new Error(`free-text-annotation-placement: Esc did not cancel without creating\n${state.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("free-text-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
