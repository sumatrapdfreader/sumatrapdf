// The ink button's drop-down picks the color of the next stroke and how thick
// it is: a preview of the stroke and a Thickness slider below the colors. The
// slider is Annotations.InkBorderWidth, and the stroke drawn after it is set
// is that many points wide.
//
// Run: bun tests/ink-thickness.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  clientToScreen,
  findTopWindow,
  getClientRect,
  getWindowRect,
  isWindowVisible,
  MK_LBUTTON,
  MK_RBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import { clickAt, findCanvas, findChildByClass, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const MAIN_TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const HOVER_MENU_CLASS = "SumatraToolbarHoverMenu";
// the widest the slider goes, kInkThicknessMax in Toolbar.cpp
const MAX_THICKNESS = 16;

type Rect = { x: number; y: number; dx: number; dy: number };

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

async function toolbarDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
}

// the visible annotation button for a command, in toolbar client coords
function annotButtonRect(raw: string, cmd: number): Rect | null {
  const re = /annotation-idx=\d+ cmd=(\d+) hidden=(\d) enabled=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    if (+m[1]! === cmd && m[2] === "0") {
      const x = +m[3]!;
      const y = +m[4]!;
      return { x, y, dx: +m[5]! - x, dy: +m[6]! - y };
    }
  }
  return null;
}

// right-click opens the drop-down at once, without waiting for the hover delay.
// The cursor has to be on the button or the drop-down closes itself.
function rightClickToolbar(toolbar: number, x: number, y: number): void {
  const s = clientToScreen(toolbar, x, y);
  setCursorPos(s.x, s.y);
  const lp = packCoords(x, y);
  sendMessage(toolbar, WM_RBUTTONDOWN, MK_RBUTTON, lp);
  sendMessage(toolbar, WM_RBUTTONUP, 0, lp);
}

async function inkAnnotWidth(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  const m = /ink strokes=\d+ points=\d+ opacity=\d+ width=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`ink-thickness: no ink annotation in the dump\n${raw}`);
  }
  return +m[1]!;
}

async function drawStroke(canvas: number, pts: { x: number; y: number }[]): Promise<void> {
  const first = pts[0]!;
  const s = clientToScreen(canvas, first.x, first.y);
  setCursorPos(s.x, s.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(first.x, first.y));
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(first.x, first.y));
  await sleep(50);
  for (let i = 1; i < pts.length; i++) {
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(pts[i]!.x, pts[i]!.y));
  }
  const last = pts[pts.length - 1]!;
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(last.x, last.y));
  await sleep(100 * SLOW_BUILD_FACTOR);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ink-thickness");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "Annotations [",
      "\tPresetColors = #ff0000 #00ff00",
      "\tInkColor = #00ff00",
      "\tInkBorderWidth = 3",
      "]",
      "",
    ].join("\n"),
  );
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(600 * SLOW_BUILD_FACTOR);
    const toolbar = findChildByClass(frame, MAIN_TOOLBAR_CLASS);
    const canvas = findCanvas(frame);

    const btn = annotButtonRect(await toolbarDump(client), cmdId("CmdCreateAnnotInk"));
    if (!btn) {
      throw new Error("ink-thickness: no ink button on the Edit PDF toolbar");
    }
    rightClickToolbar(toolbar, btn.x + (btn.dx >> 1), btn.y + (btn.dy >> 1));
    await sleep(400 * SLOW_BUILD_FACTOR);

    const dump = await toolbarDump(client);
    // the colors come first, the slider below them with the width in use
    const item =
      /dropdown-item idx=\d+ cmd=\d+ current=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=thickness=(\d+)/.exec(dump);
    if (!item) {
      throw new Error(`ink-thickness: the ink drop-down has no thickness slider\n${dump}`);
    }
    if (item[5] !== "3") {
      throw new Error(`ink-thickness: the slider opened at ${item[5]}, want the setting's 3`);
    }
    const colors = [...dump.matchAll(/dropdown-item idx=\d+ cmd=\d+ current=(\d) rect=[-\d,]+ text=(#[0-9a-f]+)/g)];
    const current = colors.filter((m) => m[1] === "1").map((m) => m[2]);
    if (current.length !== 1 || current[0] !== "#00ff00") {
      throw new Error(`ink-thickness: the color in use is ${current.join(" ")}, want #00ff00`);
    }

    // dragged all the way to Thick, the next stroke is as wide as it goes
    const menu = findTopWindow(pid, HOVER_MENU_CLASS);
    if (!menu || !isWindowVisible(menu)) {
      throw new Error("ink-thickness: the ink drop-down did not open");
    }
    const mr = getWindowRect(menu);
    const sy = (+item[2]! + +item[4]!) >> 1;
    await clickAt(menu, +item[3]! - 1 - mr.left, sy - mr.top);
    await sleep(300 * SLOW_BUILD_FACTOR);

    const canvasRect = getClientRect(canvas);
    const cx = Math.floor(canvasRect.right / 2);
    const cy = Math.floor(canvasRect.bottom / 2);
    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await sleep(300 * SLOW_BUILD_FACTOR);
    await drawStroke(canvas, [
      { x: cx - 60, y: cy },
      { x: cx - 20, y: cy + 20 },
      { x: cx + 20, y: cy - 20 },
      { x: cx + 60, y: cy },
    ]);
    await client.waitForRenderIdle();
    const width = await inkAnnotWidth(client);
    if (width !== MAX_THICKNESS) {
      throw new Error(`ink-thickness: the stroke is ${width} points wide, want ${MAX_THICKNESS}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("ink-thickness: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
