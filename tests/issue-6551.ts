// #6551: editing a free text grew its box even when the text still fit, so a
// snug annotation (box smaller than our padded estimate) got bigger on every
// edit and its background covered what was underneath. Typing one more char
// into a box with room to spare must keep the box as it was.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  enumChildWindows,
  getClassName,
  getControlText,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  VK_END,
  WM_CHAR,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

// 12pt text in a 13pt tall box: snug, as many PDFs have them
const kRect = "85 400 245 413";

type State = { raw: string; rect: string; screen: { x: number; y: number; dx: number; dy: number }; edit: boolean };

async function state(client: ControlClient): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const ft = /type=FreeText[^\n]*?rect=(\S+) screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    const edit = /freeTextEdit active=(\d)/.exec(raw);
    if (res[0] === 0 && ft && edit) {
      const screen = { x: +ft[2]!, y: +ft[3]!, dx: +ft[4]!, dy: +ft[5]! };
      return { raw, rect: ft[1]!, screen, edit: edit[1] === "1" };
    }
    if (Date.now() > deadline) throw new Error(`issue-6551: can't read the annotation state\n${raw}`);
    await sleep(50);
  }
}

async function waitForEdit(client: ControlClient, active: boolean, what: string): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await state(client);
    if (s.edit === active) return s;
    if (Date.now() > deadline) throw new Error(`issue-6551: ${what}\n${s.raw}`);
    await sleep(40);
  }
}

async function doubleClick(client: ControlClient, canvas: number, x: number, y: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    return;
  }
  const s = clientToScreen(canvas, x, y);
  setCursorPos(s.x, s.y);
  const pt = packCoords(x, y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, pt);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, pt);
  sendMessage(canvas, WM_LBUTTONUP, 0, pt);
  sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, pt);
  sendMessage(canvas, WM_LBUTTONUP, 0, pt);
}

// orig edits in an Edit child of the canvas, ng in a text area on the frame
function editTarget(frame: number, canvas: number): number {
  if (USE_NG) return frame;
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (getClassName(hwnd) === "Edit" && getControlText(hwnd).startsWith("asas")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  if (!found) throw new Error("issue-6551: in-place edit box not found");
  return found;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6551");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "freetext.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      `<< /Type /Annot /Subtype /FreeText /P 3 0 R /Rect [${kRect}] /Contents (asas101) /DA (/Helv 12 Tf 0 g) /BS << /W 0 >> >>`,
    ]),
    "latin1",
  );
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await client.waitForRenderIdle();
    await sleep(200);
    const before = await state(client);

    const sc = before.screen;
    await doubleClick(client, canvas, sc.x + Math.floor(sc.dx / 4), sc.y + Math.floor(sc.dy / 2));
    await waitForEdit(client, true, "double-click did not edit the free text");
    const box = editTarget(frame, canvas);
    sendMessage(box, WM_KEYDOWN, VK_END, 0);
    sendMessage(box, WM_KEYUP, VK_END, 0);
    sendMessage(box, WM_CHAR, "1".charCodeAt(0), 0);
    // Ctrl+Enter reaches the edit control as LF
    sendMessage(box, WM_CHAR, 0x0a, 0);
    await waitForEdit(client, false, "the edit did not commit");
    await client.waitForRenderIdle();

    const after = await state(client);
    if (!/undo canUndo=1 canRedo=0 modified=1/.test(after.raw)) {
      throw new Error(`issue-6551: the edit was not committed\n${after.raw}`);
    }
    if (after.rect !== before.rect) {
      throw new Error(`issue-6551: editing the free text resized it: ${before.rect} -> ${after.rect}`);
    }

    // Text that wraps grows the height while keeping the top-left corner and width.
    const sc2 = after.screen;
    await doubleClick(client, canvas, sc2.x + Math.floor(sc2.dx / 4), sc2.y + Math.floor(sc2.dy / 2));
    await waitForEdit(client, true, "the second double-click did not edit the free text");
    const box2 = editTarget(frame, canvas);
    sendMessage(box2, WM_KEYDOWN, VK_END, 0);
    sendMessage(box2, WM_KEYUP, VK_END, 0);
    for (const ch of " and some more text to go past the right edge") {
      sendMessage(box2, WM_CHAR, ch.charCodeAt(0), 0);
    }
    sendMessage(box2, WM_CHAR, 0x0a, 0);
    await waitForEdit(client, false, "the second edit did not commit");
    await client.waitForRenderIdle();
    const taller = await state(client);
    const [x0, y0, dx0, dy0] = after.rect.split(",").map(Number);
    const [x1, y1, dx1, dy1] = taller.rect.split(",").map(Number);
    if (
      Math.abs(dx1! - dx0!) > 0.01 ||
      Math.abs(x1! - x0!) > 0.01 ||
      Math.abs(y1! - y0!) > 0.01 ||
      !(dy1! > dy0! + 0.01)
    ) {
      throw new Error(`issue-6551: wrapping must only increase the height: ${after.rect} -> ${taller.rect}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
