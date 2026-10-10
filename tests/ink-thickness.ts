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
import { IS_MAC } from "./host.ts";
import { cmdId, pollUntil, runStandalone, tmpPath, USE_NG, makeBlankPdf } from "./util.ts";
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
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import {
  clickAt,
  findCanvas,
  findChildByClass,
  killAndWait,
  launchControlled,
  sendCommandSync,
} from "./win-automation.ts";

const MAIN_TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const HOVER_MENU_CLASS = "SumatraToolbarHoverMenu";
const ANNOT_TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const POPUP_CLASS = "SumatraAnnotColorPopup";
// the widest the slider goes, kInkThicknessMax in Toolbar.cpp
const MAX_THICKNESS = 16;

type Rect = { x: number; y: number; dx: number; dy: number };

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

// ng has no popup HWND. A posted down/up pair can have a cursor snap between
// them, which gpui treats as a drag and the control never receives the click.
async function ngClick(client: ControlClient, x: number, y: number, button = 0): Promise<void> {
  // The drop-down closes when the real cursor is somewhere else.
  if (IS_MAC) {
    const at = clientToScreen(0, x, y);
    setCursorPos(at.x, at.y);
  }
  const res = await client.request(ControlCommand.TestInput, ["click", x, y, button, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`ink-thickness: click failed: ${raw}`);
  }
}

type ThicknessHit = { value: string; x: number; y: number; x2: number; y2: number };

// the ink slider in frame dips, once it has been laid out
function thicknessDip(raw: string): ThicknessHit | null {
  const item =
    /dropdown-item idx=(\d+) cmd=\d+ current=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=thickness=(\d+)/.exec(raw);
  if (!item) {
    return null;
  }
  const dip = new RegExp(`^dropdown-dip idx=${item[1]} rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`, "m").exec(raw);
  if (!dip) {
    return null;
  }
  const x = +dip[1]!;
  const y = +dip[2]!;
  const x2 = +dip[3]!;
  const y2 = +dip[4]!;
  if (x2 - x < 8 || y2 - y < 1) {
    return null;
  }
  return { value: item[6]!, x, y, x2, y2 };
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
  const x = btn.x + (btn.dx >> 1);
  const y = btn.y + (btn.dy >> 1);
  rightClickToolbar(toolbar, x, y);
  const re = /dropdown-item idx=\d+ cmd=\d+ current=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=thickness=(\d+)/;
  const dump = await pollUntil(
    () => toolbarDump(client),
    (s) => re.test(s),
    {
      error: (s) => `ink-thickness: ${what}\n${s}`,
    },
  );
  return re.exec(dump)!;
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

async function inkActive(client: ControlClient): Promise<boolean> {
  const raw = await markupDump(client);
  const m = /inkPlacement active=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`ink-thickness: no ink placement state\n${raw}`);
  }
  return m[1] === "1";
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

async function drawStrokeNg(client: ControlClient, pts: { x: number; y: number }[]): Promise<void> {
  const first = pts[0]!;
  await client.request(ControlCommand.TestInput, ["down", first.x, first.y, 0, 0]);
  for (let i = 1; i < pts.length; i++) {
    await client.request(ControlCommand.TestInput, ["move", pts[i]!.x, pts[i]!.y, 1, 0]);
  }
  const last = pts[pts.length - 1]!;
  await client.request(ControlCommand.TestInput, ["up", last.x, last.y, 0, 0]);
}

function drawStroke(canvas: number, pts: { x: number; y: number }[]): void {
  const first = pts[0]!;
  const s = clientToScreen(canvas, first.x, first.y);
  setCursorPos(s.x, s.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(first.x, first.y));
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(first.x, first.y));
  for (let i = 1; i < pts.length; i++) {
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(pts[i]!.x, pts[i]!.y));
  }
  const last = pts[pts.length - 1]!;
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(last.x, last.y));
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
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    const toolbar = findChildByClass(frame, MAIN_TOOLBAR_CLASS);
    const canvas = findCanvas(frame);

    const btn = await pollUntil(
      async () => annotButtonRect(await toolbarDump(client), cmdId("CmdCreateAnnotInk")),
      (b) => !!b && b.dx > 0 && b.dy > 0,
      { error: "ink-thickness: no ink button on the Edit PDF toolbar" },
    );
    let cx = 0;
    let cy = 0;
    if (USE_NG) {
      const x = btn.x + (btn.dx >> 1);
      const y = btn.y + (btn.dy >> 1);
      await ngClick(client, x, y, 1);
      const dump = await pollUntil(
        () => toolbarDump(client),
        (s) => thicknessDip(s) !== null,
        {
          error: (s) => `ink-thickness: the ink drop-down has no thickness slider\n${s}`,
        },
      );
      const hit = thicknessDip(dump)!;
      if (hit.value !== "3") {
        throw new Error(`ink-thickness: the slider opened at ${hit.value}, want the setting's 3`);
      }
      const colors = [...dump.matchAll(/dropdown-item idx=\d+ cmd=\d+ current=(\d) rect=[-\d,]+ text=(#[0-9a-f]+)/g)];
      const current = colors.filter((m) => m[1] === "1").map((m) => m[2]);
      if (current.length !== 1 || current[0] !== "#00ff00") {
        throw new Error(`ink-thickness: the color in use is ${current.join(" ")}, want #00ff00`);
      }
      await ngClick(client, hit.x2 - 1, (hit.y + hit.y2) >> 1);
      const ui = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
      const canvasRc = /canvas=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(ui);
      if (!canvasRc) {
        throw new Error(`ink-thickness: no canvas in the ui state\n${ui}`);
      }
      cx = +canvasRc[1]! + (+canvasRc[3]! >> 1);
      cy = +canvasRc[2]! + (+canvasRc[4]! >> 1);
    } else {
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
      await clickAt(menu, +item[3]! - 1 - mr.left, sy - mr.top, 0);

      const canvasRect = getClientRect(canvas);
      cx = Math.floor(canvasRect.right / 2);
      cy = Math.floor(canvasRect.bottom / 2);
    }
    const stroke = [
      { x: cx - 60, y: cy },
      { x: cx - 20, y: cy + 20 },
      { x: cx + 20, y: cy - 20 },
      { x: cx + 60, y: cy },
    ];
    sendCommandSync(frame, cmdId("CmdCreateAnnotInk"));
    if (USE_NG) {
      await drawStrokeNg(client, stroke);
    } else {
      drawStroke(canvas, stroke);
    }
    await client.waitForRenderIdle();
    const width = await inkAnnotWidth(client);
    if (width !== MAX_THICKNESS) {
      throw new Error(`ink-thickness: the stroke is ${width} points wide, want ${MAX_THICKNESS}`);
    }

    // the ink tool stays on for another stroke; Esc leaves it, and a click on
    // the stroke selects it
    postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    await pollUntil(
      () => inkActive(client),
      (active) => !active,
      {
        error: "ink-thickness: Esc did not leave the ink tool",
      },
    );
    if (USE_NG) {
      await ngClick(client, cx - 60, cy);
    } else {
      await clickAt(canvas, cx - 60, cy, 0);
    }
    await pollUntil(
      async () => /annotEditToolbar .*/.exec(await markupDump(client))?.[0] ?? "",
      (line) => /annotEditToolbar visible=1/.test(line),
      { error: "ink-thickness: selecting the stroke did not show its properties" },
    );

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
    let chip = parseRect(/[=;]color:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(chips.line));
    if (USE_NG && chip.dx <= 0) {
      const ready = await pollUntil(
        async () => /annotEditToolbar .*/.exec(await markupDump(client))?.[0] ?? "",
        (line) => {
          const m = /[=;]color:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line);
          return !!m && +m[3]! > 0;
        },
        { error: "ink-thickness: the color chip has no rect" },
      );
      chip = parseRect(/[=;]color:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(ready));
    }
    if (USE_NG) {
      await ngClick(client, chip.x + (chip.dx >> 1), chip.y + (chip.dy >> 1));
    } else {
      const annotToolbar = findTopWindow(pid, ANNOT_TOOLBAR_CLASS);
      if (!annotToolbar) {
        throw new Error("ink-thickness: no annotation property row window");
      }
      await clickAt(annotToolbar, chip.x - placed.x + (chip.dx >> 1), chip.y - placed.y + (chip.dy >> 1), 0);
    }
    const thickRe = /thickness=(\d+):(-?\d+),(-?\d+),(\d+),(\d+)/;
    const popupLine = await pollUntil(
      async () => /annotColorPopup .*/.exec(await markupDump(client))?.[0] ?? "",
      (line) => {
        const m = thickRe.exec(line);
        return !!m && +m[4]! > 8;
      },
      { error: (line) => `ink-thickness: the color chip's drop-down has no thickness slider: ${line}` },
    );
    const th = thickRe.exec(popupLine);
    if (!th) {
      throw new Error(`ink-thickness: the color chip's drop-down has no thickness slider: ${popupLine}`);
    }
    if (+th[1]! !== MAX_THICKNESS) {
      throw new Error(`ink-thickness: the slider is at ${th[1]}, want the stroke's ${MAX_THICKNESS}`);
    }

    // dragged back to Thin, the stroke itself gets thinner
    const slider = { x: +th[2]!, y: +th[3]!, dx: +th[4]!, dy: +th[5]! };
    if (USE_NG) {
      await ngClick(client, slider.x + 1, slider.y + (slider.dy >> 1));
    } else {
      const popup = findTopWindow(pid, POPUP_CLASS);
      if (!popup || !isWindowVisible(popup)) {
        throw new Error("ink-thickness: the color chip's drop-down did not open");
      }
      const pr = getWindowRect(popup);
      await clickAt(popup, slider.x - pr.left, slider.y + (slider.dy >> 1) - pr.top, 0);
    }
    await client.waitForRenderIdle();
    const thin = await pollUntil(
      () => inkAnnotWidth(client),
      (width) => width === 1,
      {
        error: (width) => `ink-thickness: the stroke stayed ${width} points wide after Thin`,
      },
    );
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
