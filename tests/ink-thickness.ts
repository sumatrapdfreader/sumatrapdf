// The ink button's drop-down picks the color of the next stroke and how thick
// it is: a preview of the stroke and a Thickness slider below the colors. The
// slider is Annotations.InkBorderWidth, and the stroke drawn after it is set
// is that many points wide.
//
// A selected ink annotation's color chip opens the same drop-down, where the
// slider is that annotation's own width, so it has no Border Width chip.
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
  postMessage,
  sendMessage,
  setCursorPos,
  sleep,
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import { clickAt, findCanvas, findChildByClass, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const MAIN_TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const HOVER_MENU_CLASS = "SumatraToolbarHoverMenu";
const ANNOT_TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const POPUP_CLASS = "SumatraAnnotColorPopup";
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
function rightClickToolbar(toolbar: number, x: number, y: number): void {
  const s = clientToScreen(toolbar, x, y);
  setCursorPos(s.x, s.y);
  const lp = packCoords(x, y);
  sendMessage(toolbar, WM_MOUSEMOVE, 0, lp);
  sendMessage(toolbar, WM_RBUTTONDOWN, MK_RBUTTON, lp);
  sendMessage(toolbar, WM_RBUTTONUP, 0, lp);
}

async function waitInkDropdown(
  client: ControlClient,
  toolbar: number,
  btn: Rect,
  what: string,
): Promise<RegExpExecArray> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  let dump = "";
  const x = btn.x + (btn.dx >> 1);
  const y = btn.y + (btn.dy >> 1);
  for (;;) {
    rightClickToolbar(toolbar, x, y);
    await sleep(50);
    dump = await toolbarDump(client);
    const item =
      /dropdown-item idx=\d+ cmd=\d+ current=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=thickness=(\d+)/.exec(dump);
    if (item) {
      return item;
    }
    if (Date.now() > deadline) {
      throw new Error(`ink-thickness: ${what}\n${dump}`);
    }
    await sleep(50);
  }
}

async function markupDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

async function inkAnnotWidth(client: ControlClient): Promise<number> {
  const raw = await markupDump(client);
  const m = /ink strokes=\d+ points=\d+ opacity=\d+ width=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`ink-thickness: no ink annotation in the dump\n${raw}`);
  }
  return +m[1]!;
}

function parseRect(m: RegExpMatchArray | null): Rect {
  if (!m) {
    throw new Error("ink-thickness: no rect in the dump");
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

// the chips of the selected annotation's property row
async function annotChips(client: ControlClient): Promise<{ names: string[]; line: string }> {
  const raw = await markupDump(client);
  const line = /annotEditToolbar .*/.exec(raw)?.[0] ?? "";
  if (!/annotEditToolbar visible=1/.test(line)) {
    throw new Error(`ink-thickness: the annotation property row is not up\n${raw}`);
  }
  return { names: (/ items=(\S+)/.exec(line)?.[1] ?? "").split(","), line };
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
    const item = await waitInkDropdown(client, toolbar, btn, "the ink drop-down has no thickness slider");
    const dump = await toolbarDump(client);
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

    // the ink tool stays on for another stroke; Esc leaves it, and a click on
    // the stroke selects it
    postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    await sleep(300 * SLOW_BUILD_FACTOR);
    await clickAt(canvas, cx - 60, cy);
    await sleep(400 * SLOW_BUILD_FACTOR);

    // its color chip's drop-down has the slider, set to the annotation's own
    // width, and there is no Border Width chip
    const chips = await annotChips(client);
    if (!chips.names.includes("color")) {
      throw new Error(`ink-thickness: a selected ink stroke has no color chip: ${chips.names.join(",")}`);
    }
    if (chips.names.includes("border")) {
      throw new Error(`ink-thickness: a selected ink stroke still has a border chip: ${chips.names.join(",")}`);
    }
    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(chips.line));
    const chip = parseRect(/[=;]color:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(chips.line));
    const annotToolbar = findTopWindow(pid, ANNOT_TOOLBAR_CLASS);
    if (!annotToolbar) {
      throw new Error("ink-thickness: no annotation property row window");
    }
    await clickAt(annotToolbar, chip.x - placed.x + (chip.dx >> 1), chip.y - placed.y + (chip.dy >> 1));
    await sleep(500 * SLOW_BUILD_FACTOR);

    const popupLine = /annotColorPopup .*/.exec(await markupDump(client))?.[0] ?? "";
    const th = /thickness=(\d+):(-?\d+),(-?\d+),(\d+),(\d+)/.exec(popupLine);
    if (!th) {
      throw new Error(`ink-thickness: the color chip's drop-down has no thickness slider: ${popupLine}`);
    }
    if (+th[1]! !== MAX_THICKNESS) {
      throw new Error(`ink-thickness: the slider is at ${th[1]}, want the stroke's ${MAX_THICKNESS}`);
    }

    // dragged back to Thin, the stroke itself gets thinner
    const popup = findTopWindow(pid, POPUP_CLASS);
    if (!popup || !isWindowVisible(popup)) {
      throw new Error("ink-thickness: the color chip's drop-down did not open");
    }
    const pr = getWindowRect(popup);
    const slider = { x: +th[2]!, y: +th[3]!, dx: +th[4]!, dy: +th[5]! };
    await clickAt(popup, slider.x - pr.left, slider.y + (slider.dy >> 1) - pr.top);
    await sleep(500 * SLOW_BUILD_FACTOR);
    await client.waitForRenderIdle();
    const thin = await inkAnnotWidth(client);
    if (thin !== 1) {
      throw new Error(`ink-thickness: the stroke is ${thin} points wide after Thin, want 1`);
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
