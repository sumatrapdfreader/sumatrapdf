// Undo / redo of PDF edits (MuPDF's journal). Checks that a change can be
// stepped back and forward, that Undo / Redo are only offered when there is
// something to step to, and that one gesture is one step: a paste writes the
// annotation plus a handful of properties, a resize drag rewrites it on every
// mouse move, and both must come back with a single Undo.
//
// Also covers the buttons this state drives at the end of the Edit PDF
// toolbar: Undo, Redo and Save (Save to a new PDF and Discard live in the
// Save button's hover drop-down). They are only enabled when there is
// something to do.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  WM_COMMAND,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Square = { x: number; y: number; dx: number; dy: number };
type State = {
  raw: string;
  annotations: number;
  canUndo: boolean;
  canRedo: boolean;
  modified: boolean;
  squares: Square[];
};

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [1 0 0] /IC [1 1 0.6] /BS << /W 2 >> >>",
  ];
  return assemblePdf(objs);
}

async function state(client: ControlClient): Promise<State> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const count = /annotations=(\d+)/.exec(raw);
    const undo = /undo canUndo=(\d) canRedo=(\d) modified=(\d)/.exec(raw);
    if (res[0] === 0 && count && undo) {
      const squares: Square[] = [];
      const re = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
      for (const m of raw.matchAll(re)) {
        squares.push({ x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! });
      }
      return {
        raw,
        annotations: +count[1]!,
        canUndo: undo[1] === "1",
        canRedo: undo[2] === "1",
        modified: undo[3] === "1",
        squares,
      };
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-undo-redo: could not read markup state\n${raw}`);
    }
    await sleep(50);
  }
}

async function runCmd(client: ControlClient, frame: number, name: string, x?: number, y?: number): Promise<void> {
  if (!IS_MAC) {
    if (x === undefined) {
      sendCommandSync(frame, cmdId(name));
    } else {
      sendMessage(frame, WM_COMMAND, cmdId(name), packCoords(x, y));
    }
    return;
  }
  const args = x === undefined ? [name] : [name, x, y];
  const res = await client.request(ControlCommand.TestInvokeCommand, args);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw.startsWith("ERR") || raw.startsWith("NOTREADY")) {
    throw new Error(`annot-undo-redo: ${name} failed: ${raw || res[0]}`);
  }
}

// The markup dump's screen rect is frame pixels. A command point is canvas
// pixels; a TestInput point is frame dips.
async function framePoint(
  client: ControlClient,
  x: number,
  y: number,
): Promise<{ x: number; y: number; dipX: number; dipY: number }> {
  const layout = String((await client.request(ControlCommand.TestLayout, []))[1] ?? "");
  const scaleM = /scale=([0-9.]+)/.exec(layout);
  const canvasM = /item name=canvas visible=\d+ rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(layout);
  const scale = scaleM ? Number(scaleM[1]) : 1;
  const ox = canvasM ? Number(canvasM[1]) / scale : 0;
  const oy = canvasM ? Number(canvasM[2]) / scale : 0;
  return {
    x: Math.round(x - ox),
    y: Math.round(y - oy),
    dipX: Math.round(x * scale),
    dipY: Math.round(y * scale),
  };
}

async function clickPoint(client: ControlClient, canvas: number, x: number, y: number): Promise<void> {
  if (!IS_MAC) {
    await clickAt(canvas, x, y, 0);
    return;
  }
  const pt = await framePoint(client, x, y);
  const res = await client.request(ControlCommand.TestInput, ["click", pt.dipX, pt.dipY, 0, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || raw.startsWith("ERR")) {
    throw new Error(`annot-undo-redo: click failed: ${raw || res[0]}`);
  }
}

async function dragResize(client: ControlClient, canvas: number, corner: { x: number; y: number }): Promise<void> {
  if (!IS_MAC) {
    const start = clientToScreen(canvas, corner.x, corner.y);
    setCursorPos(start.x, start.y);
    sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(corner.x, corner.y));
    for (let i = 1; i <= 6; i++) {
      const p = { x: corner.x + i * 6, y: corner.y + i * 6 };
      const sp = clientToScreen(canvas, p.x, p.y);
      setCursorPos(sp.x, sp.y);
      sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(p.x, p.y));
    }
    sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(corner.x + 36, corner.y + 36));
    return;
  }
  const downPt = await framePoint(client, corner.x, corner.y);
  const down = await client.request(ControlCommand.TestInput, ["down", downPt.dipX, downPt.dipY, 0, 0]);
  if (down[0] !== 0) {
    throw new Error(`annot-undo-redo: drag down failed: ${down[1]}`);
  }
  for (let i = 1; i <= 6; i++) {
    const p = await framePoint(client, corner.x + i * 6, corner.y + i * 6);
    await client.request(ControlCommand.TestInput, ["move", p.dipX, p.dipY, 1, 0]);
  }
  const up = await framePoint(client, corner.x + 36, corner.y + 36);
  await client.request(ControlCommand.TestInput, ["up", up.dipX, up.dipY, 0, 0]);
}

async function undo(client: ControlClient, frame: number): Promise<State> {
  await runCmd(client, frame, "CmdUndo");
  await client.waitForRenderIdle();
  return state(client);
}

async function redo(client: ControlClient, frame: number): Promise<State> {
  await runCmd(client, frame, "CmdRedo");
  await client.waitForRenderIdle();
  return state(client);
}

type Button = { idx: number; enabled: boolean; tip: string };

// the Edit PDF toolbar buttons, by command name
async function toolbarButtons(client: ControlClient): Promise<Map<string, Button>> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const res = new Map<string, Button>();
  const re = /annotation-idx=(\d+) cmd=(\d+) hidden=(\d) enabled=(\d) rect=\S+ text=.*? tip=(.*)/g;
  for (const m of raw.matchAll(re)) {
    const cmd = +m[2]!;
    for (const name of ["CmdUndo", "CmdRedo", "CmdSaveAnnotations"]) {
      if (cmd === cmdId(name) && m[3] === "0") {
        res.set(name, { idx: +m[1]!, enabled: m[4] === "1", tip: m[5]!.trim() });
      }
    }
  }
  return res;
}

function wantButton(btns: Map<string, Button>, name: string, enabled: boolean): Button {
  const b = btns.get(name);
  if (!b) {
    throw new Error(`annot-undo-redo: ${name} is not on the Edit PDF toolbar`);
  }
  if (b.enabled !== enabled) {
    throw new Error(`annot-undo-redo: ${name} is ${b.enabled ? "enabled" : "disabled"}, want the opposite`);
  }
  return b;
}

function want(s: State, what: string, cond: boolean): void {
  if (!cond) {
    throw new Error(`annot-undo-redo: ${what}\n${s.raw}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-undo-redo");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "square.pdf");
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
    await client.setNotificationsEnabled(false);
    const canvas = IS_MAC ? 0 : findCanvas(frame);
    if (!IS_MAC && !canvas) {
      throw new Error("annot-undo-redo: no canvas");
    }
    await runCmd(client, frame, "CmdToggleEditPDF");

    let s = await state(client);
    want(s, "expected one square on the page", s.squares.length === 1);
    want(s, "nothing was edited yet, so there is nothing to undo or redo", !s.canUndo && !s.canRedo);

    // the toolbar buttons, in order, all inert on an unedited document
    let btns = await toolbarButtons(client);
    const undoBtn = wantButton(btns, "CmdUndo", false);
    const redoBtn = wantButton(btns, "CmdRedo", false);
    const saveBtn = wantButton(btns, "CmdSaveAnnotations", false);
    if (!(undoBtn.idx < redoBtn.idx && redoBtn.idx < saveBtn.idx)) {
      throw new Error(
        `annot-undo-redo: want Undo, Redo, Save in that order, got ` + `${undoBtn.idx}, ${redoBtn.idx}, ${saveBtn.idx}`,
      );
    }
    if (!/square\.pdf/.test(saveBtn.tip)) {
      throw new Error(`annot-undo-redo: Save tooltip must name the file, got "${saveBtn.tip}"`);
    }

    if (IS_MAC) {
      // Fit page keeps moving the square for a moment after the first idle.
      let prev = s.squares[0]!;
      for (let i = 0; i < 20; i++) {
        await sleep(50);
        s = await state(client);
        const cur = s.squares[0]!;
        if (
          Math.abs(cur.x - prev.x) <= 1 &&
          Math.abs(cur.y - prev.y) <= 1 &&
          Math.abs(cur.dx - prev.dx) <= 1 &&
          Math.abs(cur.dy - prev.dy) <= 1
        ) {
          break;
        }
        prev = cur;
      }
    }
    const original = s.squares[0]!;
    const mid = { x: original.x + Math.floor(original.dx / 2), y: original.y + Math.floor(original.dy / 2) };
    const hit = IS_MAC ? await framePoint(client, mid.x, mid.y) : { x: mid.x, y: mid.y, dipX: mid.x, dipY: mid.y };

    // delete -> undo -> redo -> undo
    await runCmd(client, frame, "CmdDeleteAnnotation", hit.x, hit.y);
    await client.waitForRenderIdle();
    s = await state(client);
    want(s, "the annotation was not deleted", s.annotations === 0);
    want(s, "a delete must be undoable", s.canUndo && !s.canRedo);
    want(s, "a delete leaves unsaved changes", s.modified);
    btns = await toolbarButtons(client);
    wantButton(btns, "CmdUndo", true);
    wantButton(btns, "CmdRedo", false);
    wantButton(btns, "CmdSaveAnnotations", true);

    s = await undo(client, frame);
    want(s, "undo did not bring the annotation back", s.annotations === 1 && s.squares.length === 1);
    want(s, "after undoing the only change there is nothing left to undo", !s.canUndo && s.canRedo);
    want(s, "undoing every change leaves the document as it was on disk", !s.modified);
    btns = await toolbarButtons(client);
    wantButton(btns, "CmdUndo", false);
    wantButton(btns, "CmdRedo", true);
    wantButton(btns, "CmdSaveAnnotations", false);
    const back = s.squares[0]!;
    want(
      s,
      `restored square is at ${back.x},${back.y}, not at ${original.x},${original.y}`,
      Math.abs(back.x - original.x) <= 2 && Math.abs(back.y - original.y) <= 2,
    );

    s = await redo(client, frame);
    want(s, "redo did not delete the annotation again", s.annotations === 0);
    want(s, "after redoing the last change there is nothing left to redo", s.canUndo && !s.canRedo);

    s = await undo(client, frame);
    want(s, "second undo did not bring the annotation back", s.annotations === 1);

    // a paste writes the annotation and its properties: one undo step
    await clickPoint(client, canvas, mid.x, mid.y);
    const copyAt = IS_MAC ? await framePoint(client, mid.x, mid.y) : hit;
    await runCmd(client, frame, "CmdCopyAnnotation", copyAt.x, copyAt.y);
    const pasteAt = IS_MAC ? await framePoint(client, mid.x + 150, mid.y + 100) : { x: mid.x + 150, y: mid.y + 100 };
    await runCmd(client, frame, "CmdPasteAnnotation", pasteAt.x, pasteAt.y);
    await client.waitForRenderIdle();
    s = await state(client);
    want(s, "paste did not add a second annotation", s.annotations === 2);
    s = await undo(client, frame);
    want(s, "one undo must take back the whole paste", s.annotations === 1);

    // a resize drag rewrites the annotation on every mouse move: one undo step
    s = await state(client);
    const before = s.squares[0]!;
    await clickPoint(client, canvas, before.x + Math.floor(before.dx / 2), before.y + Math.floor(before.dy / 2));
    const corner = { x: before.x + before.dx, y: before.y + before.dy };
    await dragResize(client, canvas, corner);
    await client.waitForRenderIdle();
    s = await state(client);
    const resized = s.squares[0]!;
    want(s, `the resize drag did not grow the square (${resized.dx}x${resized.dy})`, resized.dx > before.dx + 8);

    s = await undo(client, frame);
    const undone = s.squares[0]!;
    want(
      s,
      `one undo must take back the whole resize drag: ${undone.dx}x${undone.dy}, want ${before.dx}x${before.dy}`,
      Math.abs(undone.dx - before.dx) <= 2 && Math.abs(undone.dy - before.dy) <= 2,
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-undo-redo: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
