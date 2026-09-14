// The highlighter is a mode that highlights text: every text selection finished
// while it's on becomes a highlight annotation, text already selected when it's
// picked is highlighted at once, and Esc leaves it. Ink paints the way the old
// highlighter brush did: 40% yellow by default, 16 points wide.
//
// Run: bun tests/issue-6137.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getClientRect,
  MK_CONTROL,
  MK_LBUTTON,
  packCoords,
  postChar,
  postMessage,
  sendMessage,
  setCursorPos,
  sleep,
  VK_END,
  VK_RETURN,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, pressEscape, sendCommandSync } from "./win-automation.ts";

type Point = { x: number; y: number };

const HINT = "Select text to highlight it. **Esc** or **Enter** to finish.";

function makePdf(): string {
  const stream = "BT /F1 24 Tf 72 720 Td (highlight this line) Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R " +
      "/Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ]);
}

type State = {
  highlighter: boolean;
  notification: boolean;
  message: string;
  ink: boolean;
  selected: boolean;
  hover: boolean;
  highlights: number;
  annotations: number;
  screen: { x: number; y: number; dx: number; dy: number } | null;
  raw: string;
};

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const hl = /highlighterPlacement active=(\d) notification=(\d) cmd=\d+ message=(.*)/.exec(raw);
  const ink = /inkPlacement active=(\d)/.exec(raw);
  const sel = /state selected=(\d) hover=(\d)/.exec(raw);
  const count = /annotations=(\d+)/.exec(raw);
  // the first annotation's, on a line of its own under its type=
  const screen = /^screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/m.exec(raw);
  if (res[0] !== 0 || !hl || !ink || !sel || !count) {
    throw new Error(`issue-6137: could not read state\n${raw}`);
  }
  return {
    highlighter: hl[1] === "1",
    notification: hl[2] === "1",
    message: hl[3]!.trim(),
    ink: ink[1] === "1",
    selected: sel[1] === "1",
    hover: sel[2] === "1",
    highlights: (raw.match(/type=Highlight/g) ?? []).length,
    annotations: +count[1]!,
    screen: screen ? { x: +screen[1]!, y: +screen[2]!, dx: +screen[3]!, dy: +screen[4]! } : null,
    raw,
  };
}

async function waitUntil(client: ControlClient, pred: (s: State) => boolean, msg: string): Promise<State> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let s = await state(client);
  while (Date.now() < deadline) {
    if (pred(s)) {
      return s;
    }
    await sleep(40);
    s = await state(client);
  }
  throw new Error(`issue-6137: ${msg}\n${s.raw}`);
}

// the page's text line, selected with the keyboard (v, End)
async function selectLineWithKeyboard(client: ControlClient, frame: number): Promise<void> {
  const deadline = Date.now() + 4_000 * SLOW_BUILD_FACTOR;
  const waitFor = async (re: RegExp) => {
    let dump = "";
    while (Date.now() < deadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (re.test(dump)) {
        return;
      }
      await sleep(25);
    }
    throw new Error(`issue-6137: keyboard selection did not reach ${re}\n${dump}`);
  };
  sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
  await waitFor(/active=1/);
  await postChar(frame, "v");
  await waitFor(/visual=1/);
  postMessage(frame, WM_KEYDOWN, VK_END, 0);
  postMessage(frame, WM_KEYUP, VK_END, 0);
  await sleep(200);
}

async function dragSelect(canvas: number, x0: number, y0: number, x1: number, y1: number): Promise<void> {
  postMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x0, y0));
  await sleep(150);
  const steps = 8;
  for (let i = 1; i <= steps; i++) {
    const x = Math.round(x0 + ((x1 - x0) * i) / steps);
    const y = Math.round(y0 + ((y1 - y0) * i) / steps);
    postMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, y));
    await sleep(60);
  }
  postMessage(canvas, WM_LBUTTONUP, 0, packCoords(x1, y1));
  await sleep(300);
}

async function drawStroke(canvas: number, points: Point[]): Promise<void> {
  const first = points[0]!;
  const screen = clientToScreen(canvas, first.x, first.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(first.x, first.y));
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(first.x, first.y));
  await sleep(50);
  for (let i = 1; i < points.length; i++) {
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(points[i]!.x, points[i]!.y));
  }
  const last = points[points.length - 1]!;
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(last.x, last.y));
  await sleep(100);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6137");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "text.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
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
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await sleep(200);

    // text selected before picking the highlighter is highlighted right away
    await selectLineWithKeyboard(client, frame);
    sendCommandSync(frame, cmdId("CmdAnnotationHighlightBrush"));
    let s = await waitUntil(
      client,
      (st) => st.highlighter && st.highlights === 1 && st.screen !== null && st.screen.dx > 4,
      "picking the highlighter did not highlight the selection and start the mode",
    );
    if (!s.notification || s.message !== HINT) {
      throw new Error(`issue-6137: the highlighter hint is "${s.message}", want "${HINT}"\n${s.raw}`);
    }
    if (s.selected) {
      throw new Error(`issue-6137: the new highlight is selected, which would eat the next press\n${s.raw}`);
    }

    // a drag over the text makes another highlight, and the mode stays on
    await client.setNotificationsEnabled(false);
    const r = s.screen!;
    const y = r.y + Math.floor(r.dy / 2);
    await dragSelect(canvas, r.x + 2, y, r.x + r.dx - 2, y);
    s = await waitUntil(client, (st) => st.highlights === 2, "selecting text did not highlight it");
    if (!s.highlighter || s.selected) {
      throw new Error(`issue-6137: after highlighting, want the mode on and nothing selected\n${s.raw}`);
    }

    // the mode only selects text: resting on a highlight doesn't hover it, and
    // Ctrl+click (which selects it in Edit PDF) doesn't pick it
    const hx = r.x + Math.floor(r.dx / 2);
    sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(hx, y));
    await sleep(150);
    s = await state(client);
    if (s.hover) {
      throw new Error(`issue-6137: the highlighter hovers the annotation under the cursor\n${s.raw}`);
    }
    sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON | MK_CONTROL, packCoords(hx, y));
    sendMessage(canvas, WM_LBUTTONUP, MK_CONTROL, packCoords(hx, y));
    await sleep(300);
    s = await state(client);
    if (s.selected || !s.highlighter) {
      throw new Error(`issue-6137: Ctrl+click in the highlighter selected an annotation\n${s.raw}`);
    }

    // Esc leaves it; then a selection is just a selection
    await pressEscape(frame);
    s = await waitUntil(client, (st) => !st.highlighter, "Esc did not leave the highlighter");
    await dragSelect(canvas, r.x + 2, y, r.x + r.dx - 2, y);
    s = await state(client);
    if (s.highlights !== 2) {
      throw new Error(`issue-6137: a selection after Esc was highlighted\n${s.raw}`);
    }

    // Enter leaves it too
    sendCommandSync(frame, cmdId("CmdAnnotationHighlightBrush"));
    await waitUntil(client, (st) => st.highlighter, "the highlighter did not start again");
    postMessage(frame, WM_KEYDOWN, VK_RETURN, 0);
    postMessage(frame, WM_KEYUP, VK_RETURN, 0);
    await waitUntil(client, (st) => !st.highlighter, "Enter did not leave the highlighter");

    // ink: translucent 40% yellow and 16 points wide by default, stays on
    const canvasRect = getClientRect(canvas);
    const cx = Math.floor(canvasRect.right / 2);
    const cy = Math.floor(canvasRect.bottom / 2);
    sendCommandSync(frame, cmdId("CmdCreateAnnotInk"));
    await waitUntil(client, (st) => st.ink, "ink tool did not start");
    await drawStroke(canvas, [
      { x: cx - 80, y: cy + 40 },
      { x: cx - 30, y: cy + 70 },
      { x: cx + 30, y: cy + 40 },
      { x: cx + 80, y: cy + 70 },
    ]);
    s = await waitUntil(client, (st) => /ink strokes=\d+/.test(st.raw), "the ink stroke was not committed");
    const ink = /ink strokes=\d+ points=\d+ opacity=(\d+) width=(-?\d+)/.exec(s.raw)!;
    // 40% of 255
    if (+ink[1]! < 90 || +ink[1]! > 120 || +ink[2]! !== 16) {
      throw new Error(`issue-6137: ink is opacity ${ink[1]} width ${ink[2]}, want ~102 and 16\n${s.raw}`);
    }
    if (!s.ink) {
      throw new Error(`issue-6137: the ink tool did not stay on after a stroke\n${s.raw}`);
    }
    await pressEscape(frame);
    await waitUntil(client, (st) => !st.ink, "Esc did not leave the ink tool");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6137: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
