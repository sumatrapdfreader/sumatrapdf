// One gesture is one undo step (follow-up to issue #6217). Three gestures that
// wrote several journal operations: applying redaction marks on more than one
// page, committing a free text edit whose box grew, and picking a color that
// also sets the opacity.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { openChipDropdown, pickSwatch } from "./annot-color-dropdown.ts";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  enumChildWindows,
  getClassName,
  getControlText,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  WM_CHAR,
  WM_COMMAND,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type State = { raw: string; annotations: number; canUndo: boolean; modified: boolean; editActive: boolean };

type Rect = { x: number; y: number; dx: number; dy: number };

const SETTINGS = [
  "UiLanguage = en",
  "RestoreSession = false",
  "ShowStartPage = false",
  "CheckForUpdates = false",
  "Annotations [",
  "\tPresetColors = #80ff0000 #00ff00",
  "]",
  "",
].join("\n");

function pageObj(annot: number): string {
  return `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot} 0 R] >>`;
}

function redactObj(page: number): string {
  return `<< /Type /Annot /Subtype /Redact /P ${page} 0 R /Rect [72 600 300 700] /QuadPoints [72 700 300 700 72 600 300 600] >>`;
}

// two pages, one redaction mark each
function makeRedactPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    pageObj(5),
    pageObj(6),
    redactObj(3),
    redactObj(4),
  ]);
}

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function makeSquarePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    pageObj(4),
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [0 0 1] /IC [1 1 0.6] /BS << /W 2 >> >>",
  ]);
}

async function state(client: ControlClient): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const count = /annotations=(\d+)/.exec(raw);
    const undo = /undo canUndo=(\d) canRedo=(\d) modified=(\d)/.exec(raw);
    const edit = /freeTextEdit active=(\d)/.exec(raw);
    if (res[0] === 0 && count && undo) {
      return {
        raw,
        annotations: +count[1]!,
        canUndo: undo[1] === "1",
        modified: undo[3] === "1",
        editActive: edit?.[1] === "1",
      };
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-undo-one-step: could not read markup state\n${raw}`);
    }
    await sleep(50);
  }
}

async function waitFor(client: ControlClient, pred: (s: State) => boolean, what: string): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let s = await state(client);
  while (!pred(s)) {
    if (Date.now() > deadline) {
      throw new Error(`annot-undo-one-step: ${what}\n${s.raw}`);
    }
    await sleep(50);
    s = await state(client);
  }
  return s;
}

async function undo(client: ControlClient, frame: number): Promise<State> {
  sendCommandSync(frame, cmdId("CmdUndo"));
  await client.waitForRenderIdle();
  return state(client);
}

function want(s: State, what: string, cond: boolean): void {
  if (!cond) {
    throw new Error(`annot-undo-one-step: ${what}\n${s.raw}`);
  }
}

function squareOf(raw: string): Rect | null {
  const sq = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (!sq) {
    return null;
  }
  return { x: +sq[1]!, y: +sq[2]!, dx: +sq[3]!, dy: +sq[4]! };
}

function sameRect(a: Rect | null, b: Rect | null): boolean {
  return !!a && !!b && a.x === b.x && a.y === b.y && a.dx === b.dx && a.dy === b.dy;
}

function clickSquare(canvas: number, sq: Rect): Promise<void> {
  return clickAt(canvas, sq.x + Math.floor(sq.dx / 2), sq.y + Math.floor(sq.dy / 2), 0);
}

function propertiesShown(raw: string): boolean {
  return /annotEditToolbar visible=1/.test(raw);
}

// Edit PDF refits the page, so the square can move between the read and the click.
async function stableSquare(client: ControlClient): Promise<Rect> {
  await client.waitForRenderIdle();
  let prev = squareOf((await state(client)).raw);
  const deadline = Date.now() + 2000;
  for (;;) {
    await sleep(40);
    const s = await state(client);
    const cur = squareOf(s.raw);
    if (sameRect(prev, cur) && cur) {
      return cur;
    }
    if (Date.now() > deadline) {
      if (!cur) {
        throw new Error(`annot-undo-one-step: no square on the page\n${s.raw}`);
      }
      return cur;
    }
    prev = cur;
  }
}

function findEditBox(canvas: number): number {
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (getClassName(hwnd) !== "Edit" || !getControlText(hwnd).startsWith("This is a text")) {
      return true;
    }
    found = hwnd;
    return false;
  });
  return found;
}

async function launch(name: string, pdfContent: string) {
  const dir = tmpPath(`annot-undo-one-step-${name}`);
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, `${name}.pdf`);
  writeFileSync(pdf, pdfContent, "latin1");
  const launched = await launchControlled(["-appdata", appdata, "-view", "single page", "-zoom", "fit page", pdf]);
  await launched.client.waitForRenderIdle();
  await launched.client.setNotificationsEnabled(false);
  return launched;
}

// applying marks on two pages is one step
async function testRedactions(): Promise<void> {
  const { proc, client, frame } = await launch("redact", makeRedactPdf());
  try {
    let s = await state(client);
    want(s, "expected a redaction mark on each page", s.annotations === 2 && !s.canUndo);

    sendCommandSync(frame, cmdId("CmdApplyRedactions"));
    s = await waitFor(client, (st) => st.annotations === 0, "applying did not remove the marks");

    s = await undo(client, frame);
    want(s, "one undo must bring back the marks of both pages", s.annotations === 2);
    want(s, "one undo must leave nothing else to undo", !s.canUndo && !s.modified);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// a free text commit that grows the box writes the rect and the text: one step
async function testFreeText(): Promise<void> {
  const { proc, client, frame } = await launch("freetext", makeBlankPdf());
  try {
    const canvas = findCanvas(frame);
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(120, 250));
    await waitFor(client, (st) => st.editActive, "creating a free text did not open the in-place editor");
    const box = findEditBox(canvas);
    if (!box) {
      throw new Error("annot-undo-one-step: no in-place edit box");
    }
    sendText(box, "one\r\ntwo\r\nthree\r\nfour\r\nfive\r\nsix\r\nseven\r\neight");
    // Ctrl+Enter reaches the edit control as LF
    sendMessage(box, WM_CHAR, 0x0a, 0);
    await waitFor(client, (st) => !st.editActive, "the edit did not commit");
    await client.waitForRenderIdle();
    let s = await state(client);
    want(s, "the free text annotation is missing", s.annotations === 1 && s.canUndo);

    s = await undo(client, frame);
    want(s, "the first undo takes back the text, not the annotation", s.annotations === 1 && s.canUndo);
    s = await undo(client, frame);
    want(s, "the second undo must take back the creation", s.annotations === 0 && !s.canUndo && !s.modified);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// an interior color pick sets the color and the opacity: one step
async function testColorPick(): Promise<void> {
  const { proc, client, frame } = await launch("square", makeSquarePdf());
  try {
    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    const aimed = await stableSquare(client);
    await clickSquare(canvas, aimed);
    let s = await state(client);
    // one more click at the current center if the property row is still hidden
    if (!propertiesShown(s.raw)) {
      await clickSquare(canvas, squareOf(s.raw) ?? aimed);
    }
    await waitFor(
      client,
      (st) => propertiesShown(st.raw),
      `selecting the square did not show its properties\naimed=${aimed.x},${aimed.y},${aimed.dx},${aimed.dy}`,
    );

    const swatches = await openChipDropdown(client, proc.pid!, "interiorColor");
    await pickSwatch(client, proc.pid!, swatches, 1);
    s = await state(client);
    want(s, "picking a color must be undoable", s.canUndo && s.modified);

    s = await undo(client, frame);
    want(s, "one undo must take back the color pick", !s.canUndo && !s.modified);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  // ANNOT_UNDO_ONLY=redact|freetext|color runs one part
  const only = process.env.ANNOT_UNDO_ONLY;
  if (!only || only === "redact") {
    await testRedactions();
  }
  if (!only || only === "freetext") {
    await testFreeText();
  }
  if (!only || only === "color") {
    await testColorPick();
  }
  console.log("annot-undo-one-step: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
