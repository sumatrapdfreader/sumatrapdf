// Stamp, caret, and file-attachment annotations from the PDF toolbar or
// Command Palette enter a placement mode. A ghost of the annotation follows
// the cursor; canvas-margin clicks do nothing; a page click creates it; Esc
// cancels.

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

type Kind = "stamp" | "caret" | "file";

type PlacementState = {
  active: boolean;
  notification: boolean;
  cursor: boolean;
  preview: boolean;
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

function cmdName(kind: Kind): string {
  if (kind === "stamp") {
    return "CmdCreateAnnotStamp";
  }
  if (kind === "caret") {
    return "CmdCreateAnnotCaret";
  }
  return "CmdCreateAnnotFileAttachment";
}

function paletteQuery(kind: Kind): string {
  if (kind === "stamp") {
    return ">Create Stamp Annotation";
  }
  if (kind === "caret") {
    return ">Create Caret Annotation";
  }
  return ">Create File Attachment Annotation";
}

function expectedMessage(kind: Kind): string {
  if (kind === "stamp") {
    return "Place stamp annotation. **Esc** to cancel.";
  }
  if (kind === "caret") {
    return "Place caret annotation. **Esc** to cancel.";
  }
  return "Place file attachment. **Esc** to cancel.";
}

function dumpKey(kind: Kind): string {
  if (kind === "stamp") {
    return "stampPlacement";
  }
  if (kind === "caret") {
    return "caretPlacement";
  }
  return "fileAttachmentPlacement";
}

async function placementState(client: ControlClient, kind: Kind): Promise<PlacementState> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const key = dumpKey(kind);
  const state = new RegExp(
    `${key} active=(\\d+) notification=(\\d+) cursor=(\\d+) preview=(\\d+) cmd=(-?\\d+) message=(.*)`,
  ).exec(raw);
  if (res[0] !== 0 || !count || !state) {
    throw new Error(`stamp-caret-annotation-placement: could not read ${key} state\n${raw}`);
  }
  return {
    active: state[1] === "1",
    notification: state[2] === "1",
    cursor: state[3] === "1",
    preview: state[4] === "1",
    command: +state[5]!,
    annotations: +count[1]!,
    message: state[6]!.trim(),
    raw,
  };
}

async function waitForPlacement(client: ControlClient, kind: Kind, active: boolean): Promise<PlacementState> {
  const deadline = Date.now() + 5_000;
  let state: PlacementState;
  for (;;) {
    state = await placementState(client, kind);
    if (state.active === active) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`stamp-caret-annotation-placement: ${kind} active did not become ${active}\n${state.raw}`);
    }
    await sleep(40);
  }
}

function toolbarRect(dump: string, kind: Kind): { x: number; y: number; dx: number; dy: number } {
  const id = cmdId(cmdName(kind));
  const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`stamp-caret-annotation-placement: ${kind} toolbar button not found\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

async function executeFromCommandPalette(client: ControlClient, frame: number, kind: Kind): Promise<void> {
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
    throw new Error("stamp-caret-annotation-placement: command palette did not open");
  }

  const query = paletteQuery(kind);
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
      throw new Error(`stamp-caret-annotation-placement: palette did not select ${kind}\n${raw}`);
    }
    await sleep(40);
  }

  const want = cmdId(cmdName(kind));
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
  throw new Error(`stamp-caret-annotation-placement: ${kind} command was not in the filtered palette`);
}

async function testKind(
  client: ControlClient,
  frame: number,
  canvas: number,
  kind: Kind,
  annotationsBefore: number,
): Promise<number> {
  const canvasRect = getClientRect(canvas);
  const pagePoint = { x: Math.floor(canvasRect.right / 2), y: Math.floor(canvasRect.bottom / 2) };
  const wantCmd = cmdId(cmdName(kind));

  const toolbarDump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const button = toolbarRect(toolbarDump, kind);
  const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
  await clickAt(toolbar, button.x + Math.floor(button.dx / 2), button.y + Math.floor(button.dy / 2));

  let state = await waitForPlacement(client, kind, true);
  if (!state.notification || state.annotations !== annotationsBefore || state.message !== expectedMessage(kind)) {
    throw new Error(`stamp-caret-annotation-placement: ${kind} toolbar did not start placement\n${state.raw}`);
  }

  const screenPoint = clientToScreen(canvas, pagePoint.x, pagePoint.y);
  setCursorPos(screenPoint.x, screenPoint.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(pagePoint.x, pagePoint.y));
  state = await placementState(client, kind);
  if (!state.cursor || !state.preview) {
    throw new Error(`stamp-caret-annotation-placement: ${kind} preview/cursor was not active\n${state.raw}`);
  }

  await clickAt(canvas, 2, Math.floor(canvasRect.bottom / 2));
  state = await placementState(client, kind);
  if (!state.active || state.annotations !== annotationsBefore) {
    throw new Error(`stamp-caret-annotation-placement: ${kind} click outside the page ended placement\n${state.raw}`);
  }

  await clickAt(canvas, pagePoint.x, pagePoint.y);
  state = await waitForPlacement(client, kind, false);
  if (state.notification || state.annotations !== annotationsBefore + 1) {
    throw new Error(`stamp-caret-annotation-placement: ${kind} page click did not place one annotation\n${state.raw}`);
  }

  sendMessage(frame, WM_COMMAND, wantCmd, packCoords(pagePoint.x + 40, pagePoint.y + 40));
  state = await placementState(client, kind);
  if (state.active || state.notification || state.annotations !== annotationsBefore + 2) {
    throw new Error(`stamp-caret-annotation-placement: ${kind} context point did not place immediately\n${state.raw}`);
  }

  await executeFromCommandPalette(client, frame, kind);
  state = await waitForPlacement(client, kind, true);
  if (!state.notification || state.annotations !== annotationsBefore + 2 || state.command !== wantCmd) {
    throw new Error(`stamp-caret-annotation-placement: palette did not start ${kind} placement\n${state.raw}`);
  }
  await pressEscape(frame);
  state = await waitForPlacement(client, kind, false);
  if (state.notification || state.annotations !== annotationsBefore + 2) {
    throw new Error(`stamp-caret-annotation-placement: Esc did not cancel ${kind} without creating\n${state.raw}`);
  }
  return annotationsBefore + 2;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("stamp-caret-annotation-placement");
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

    let n = await testKind(client, frame, canvas, "stamp", 0);
    n = await testKind(client, frame, canvas, "caret", n);
    n = await testKind(client, frame, canvas, "file", n);
    if (n !== 6) {
      throw new Error(`stamp-caret-annotation-placement: expected 6 annotations, got ${n}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("stamp-caret-annotation-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
