// A free text annotation is edited where it sits on the page: double-clicking
// it (or the property row's Contents button) puts an edit box over it in the
// annotation's font and size, the box grows with what is typed, Enter makes a
// new line, and Ctrl+Enter writes the text back.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  clientToScreen,
  enumChildWindows,
  findTopWindow,
  getClassName,
  getControlText,
  packCoords,
  sendMessage,
  sendText,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  VK_ESCAPE,
  WM_CHAR,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };
type EditState = { active: boolean; rect: Rect; text: string; raw: string };

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

async function dump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  if (res[0] !== 0) {
    throw new Error(`free-text-in-place-edit: dump failed: ${String(res[1] ?? res[0])}`);
  }
  return String(res[1] ?? "");
}

async function editState(client: ControlClient): Promise<EditState> {
  const raw = await dump(client);
  const m = /freeTextEdit active=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=(.*)/.exec(raw);
  if (!m) {
    throw new Error(`free-text-in-place-edit: no freeTextEdit line\n${raw}`);
  }
  return {
    active: m[1] === "1",
    rect: { x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! },
    text: m[6]!,
    raw,
  };
}

async function waitForEdit(client: ControlClient, active: boolean): Promise<EditState> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await editState(client);
    if (s.active === active) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`free-text-in-place-edit: in-place edit did not become ${active}\n${s.raw}`);
    }
    await sleep(40);
  }
}

async function waitUntil(client: ControlClient, pred: (s: EditState) => boolean, msg: string): Promise<EditState> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let s = await editState(client);
  while (Date.now() < deadline) {
    if (pred(s)) {
      return s;
    }
    await sleep(40);
    s = await editState(client);
  }
  throw new Error(`free-text-in-place-edit: ${msg}\n${s.raw}`);
}

// screen rect of the selected annotation
async function selectedRect(client: ControlClient): Promise<Rect> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`free-text-in-place-edit: no selected annotation: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function doubleClickAt(canvas: number, x: number, y: number): void {
  const s = clientToScreen(canvas, x, y);
  setCursorPos(s.x, s.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(x, y));
  sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, packCoords(x, y));
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(x, y));
}

// the canvas can have other Edit children; pick the one showing `pred`
function findBox(canvas: number, pred: (text: string) => boolean): number {
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (getClassName(hwnd) !== "Edit") {
      return true;
    }
    if (pred(getControlText(hwnd))) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// WM_SETTEXT is marshalled cross-process and still runs SizeInPlaceEditToText.
// WM_CHAR to an unfocused edit does not.
function setBoxText(hwnd: number, text: string): void {
  sendText(hwnd, text.replaceAll("|", "\r\n"));
}

function pressKey(hwnd: number, vk: number): void {
  sendMessage(hwnd, WM_KEYDOWN, vk, 0);
  sendMessage(hwnd, WM_KEYUP, vk, 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("free-text-in-place-edit");
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
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    let s = await editState(client);
    if (s.active) {
      throw new Error(`free-text-in-place-edit: editing before anything was clicked\n${s.raw}`);
    }

    // creating a free text annotation opens the in-place editor on it
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(120, 250));
    s = await waitForEdit(client, true);
    if (!s.text.startsWith("This is a text")) {
      throw new Error(`free-text-in-place-edit: box shows "${s.text}", want the annotation's text`);
    }
    const annotRect = await selectedRect(client);
    if (Math.abs(s.rect.x - annotRect.x) > 3 || Math.abs(s.rect.y - annotRect.y) > 3) {
      throw new Error(
        `free-text-in-place-edit: box at ${s.rect.x},${s.rect.y} is not over the annotation ` +
          `at ${annotRect.x},${annotRect.y}\n${s.raw}`,
      );
    }
    const box = findBox(canvas, (t) => t.startsWith("This is a text"));
    if (!box) {
      throw new Error("free-text-in-place-edit: the edit control is not a child of the canvas");
    }
    const before = s.rect;

    // longer text grows the box
    const extra = " and a good deal more text than the box started with";
    setBoxText(box, s.text + extra);
    await waitUntil(
      client,
      (st) => st.text.includes("good deal") && st.rect.dx > before.dx,
      `box did not grow with the text (${before.dx})`,
    );

    // a new line
    const withLine = `${s.text}${extra}|second line`;
    setBoxText(box, withLine);
    const twoLines = await waitUntil(
      client,
      (st) => st.text.includes("|") && st.rect.dy >= before.dy,
      "Enter did not make a new line",
    );
    if (twoLines.rect.dy < before.dy) {
      throw new Error(
        `free-text-in-place-edit: box shrank below the annotation ` +
          `(${twoLines.rect.dy} < ${before.dy})\n${twoLines.raw}`,
      );
    }
    // and once the lines no longer fit, the box grows taller too
    setBoxText(box, withLine + "|".repeat(12));
    await waitUntil(client, (st) => st.rect.dy > before.dy, "box did not grow for more lines");

    // Ctrl+Enter writes the text back and goes back to the rendered annotation
    // (it reaches an edit control as a LF character)
    sendMessage(box, WM_CHAR, 0x0a, 0);
    await waitForEdit(client, false);
    await client.waitForRenderIdle();

    // the Contents button on the property row opens the same editor, and it
    // shows the text that was written back
    const after = await dump(client);
    const chip = /[=;]contents:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(after);
    const placed = / placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(after);
    if (!chip || !placed) {
      throw new Error(`free-text-in-place-edit: no Contents chip on the property row
${after}`);
    }
    const tb = findTopWindow(proc.pid!, "SumatraAnnotEditToolbar");
    if (!tb) {
      throw new Error("free-text-in-place-edit: property row window not found");
    }
    await clickAt(
      tb,
      +chip[1]! - +placed[1]! + Math.floor(+chip[3]! / 2),
      +chip[2]! - +placed[2]! + Math.floor(+chip[4]! / 2),
    );
    const chipX = +chip[1]! - +placed[1]! + Math.floor(+chip[3]! / 2);
    const chipY = +chip[2]! - +placed[2]! + Math.floor(+chip[4]! / 2);
    const reopened = await waitForEdit(client, true);
    if (!reopened.text.includes("second line")) {
      throw new Error(`free-text-in-place-edit: the typed text was not written back: "${reopened.text}"`);
    }

    // ... and the same button ends the edit
    await clickAt(tb, chipX, chipY);
    await waitForEdit(client, false);
    await client.waitForRenderIdle();

    // reopen it to check that Esc throws away what was typed since
    await clickAt(tb, chipX, chipY);
    await waitForEdit(client, true);
    const box2 = findBox(canvas, (t) => t.includes("second line"));
    if (!box2) {
      throw new Error("free-text-in-place-edit: the reopened edit control was not found");
    }
    setBoxText(box2, "throw this away");
    const replaced = await waitUntil(client, (st) => st.text === "throw this away", "could not replace the box text");
    if (replaced.text !== "throw this away") {
      throw new Error(`free-text-in-place-edit: box text was not replaced: "${replaced.text}"`);
    }
    pressKey(box2, VK_ESCAPE);
    await waitForEdit(client, false);
    await client.waitForRenderIdle();

    const annotRect2 = await selectedRect(client);
    doubleClickAt(canvas, annotRect2.x + 20, annotRect2.y + 10);
    const reopened2 = await waitForEdit(client, true);
    if (reopened2.text.includes("throw this away")) {
      throw new Error(`free-text-in-place-edit: Esc kept the discarded text: "${reopened2.text}"`);
    }
    const box3 = findBox(canvas, (t) => t.length > 0);
    pressKey(box3 || canvas, VK_ESCAPE);
    await waitForEdit(client, false);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("free-text-in-place-edit: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
