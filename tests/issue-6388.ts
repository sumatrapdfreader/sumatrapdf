// Dragging an annotation's slider (Thickness, Border Width, Text Size) updates
// the annotation while the mouse is still down, not only when it is let go.
//
// Run: bun tests/issue-6388.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  findTopWindow,
  getClientRect,
  getWindowRect,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  sendMessage,
  setCursorPos,
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const ANNOT_TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const POPUP_CLASS = "SumatraAnnotColorPopup";
const START_WIDTH = 8;

type Rect = { x: number; y: number; dx: number; dy: number };

async function markupDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

async function inkWidth(client: ControlClient): Promise<number> {
  const raw = await markupDump(client);
  const m = /ink strokes=\d+ points=\d+ opacity=\d+ width=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6388: no ink annotation in the dump\n${raw}`);
  }
  return +m[1]!;
}

function parseRect(m: RegExpMatchArray | null, what: string): Rect {
  if (!m) {
    throw new Error(`issue-6388: no ${what} rect in the dump`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function ngInput(client: ControlClient, kind: string, x: number, y: number, buttons: number): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, [kind, x, y, buttons, 0]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`issue-6388: ${kind} failed: ${raw}`);
  }
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

async function drawStrokeNg(client: ControlClient, pts: { x: number; y: number }[]): Promise<void> {
  await ngInput(client, "down", pts[0]!.x, pts[0]!.y, 0);
  for (let i = 1; i < pts.length; i++) {
    await ngInput(client, "move", pts[i]!.x, pts[i]!.y, 1);
  }
  await ngInput(client, "up", pts[pts.length - 1]!.x, pts[pts.length - 1]!.y, 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6388");
  rmSync(dir, { recursive: true, force: true });
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
      `\tInkBorderWidth = ${START_WIDTH}`,
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
    const canvas = findCanvas(frame);

    let cx = 0;
    let cy = 0;
    if (USE_NG) {
      const ui = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
      const rc = parseRect(/canvas=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(ui), "canvas");
      cx = rc.x + (rc.dx >> 1);
      cy = rc.y + (rc.dy >> 1);
    } else {
      const rc = getClientRect(canvas);
      cx = Math.floor(rc.right / 2);
      cy = Math.floor(rc.bottom / 2);
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
    if ((await inkWidth(client)) !== START_WIDTH) {
      throw new Error(`issue-6388: the stroke is not ${START_WIDTH} points wide`);
    }

    // leave the ink tool and select the stroke
    postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    await pollUntil(
      async () => /inkPlacement active=(\d)/.exec(await markupDump(client))?.[1] ?? "",
      (a) => a === "0",
      { error: "issue-6388: Esc did not leave the ink tool" },
    );
    if (USE_NG) {
      await ngInput(client, "click", cx - 60, cy, 0);
    } else {
      await clickAt(canvas, cx - 60, cy, 0);
    }
    const chipRe = /[=;]color:(-?\d+),(-?\d+),(\d+),(\d+)/;
    const line = await pollUntil(
      async () => /annotEditToolbar .*/.exec(await markupDump(client))?.[0] ?? "",
      (l) => /annotEditToolbar visible=1/.test(l) && +(chipRe.exec(l)?.[3] ?? 0) > 0,
      { error: (l) => `issue-6388: selecting the stroke did not show its color chip: ${l}` },
    );
    const chip = parseRect(chipRe.exec(line), "color chip");
    if (USE_NG) {
      await ngInput(client, "click", chip.x + (chip.dx >> 1), chip.y + (chip.dy >> 1), 0);
    } else {
      const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line), "property row");
      const row = findTopWindow(pid, ANNOT_TOOLBAR_CLASS);
      if (!row) {
        throw new Error("issue-6388: no annotation property row window");
      }
      await clickAt(row, chip.x - placed.x + (chip.dx >> 1), chip.y - placed.y + (chip.dy >> 1), 0);
    }

    const thickRe = /thickness=(\d+):(-?\d+),(-?\d+),(\d+),(\d+)/;
    const popupLine = await pollUntil(
      async () => /annotColorPopup .*/.exec(await markupDump(client))?.[0] ?? "",
      (l) => +(thickRe.exec(l)?.[4] ?? 0) > 8,
      { error: (l) => `issue-6388: the color chip's drop-down has no thickness slider: ${l}` },
    );
    const th = thickRe.exec(popupLine)!;
    const slider = { x: +th[2]!, y: +th[3]!, dx: +th[4]!, dy: +th[5]! };

    // press on the slider's current value and drag to its left end, without
    // letting go: the stroke gets thinner during the drag
    const sx = slider.x + Math.round(((START_WIDTH - 1) * slider.dx) / 15);
    const sy = slider.y + (slider.dy >> 1);
    const ex = slider.x + 1;
    let popup = 0;
    let pr = { left: 0, top: 0 };
    if (USE_NG) {
      await ngInput(client, "down", sx, sy, 0);
      await ngInput(client, "move", ex, sy, 1);
    } else {
      popup = findTopWindow(pid, POPUP_CLASS) ?? 0;
      if (!popup || !isWindowVisible(popup)) {
        throw new Error("issue-6388: the color chip's drop-down did not open");
      }
      pr = getWindowRect(popup);
      // the system sends moves at the real cursor; keep it at the drag's end
      setCursorPos(ex, sy);
      sendMessage(popup, WM_MOUSEMOVE, 0, packCoords(sx - pr.left, sy - pr.top));
      sendMessage(popup, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(sx - pr.left, sy - pr.top));
      sendMessage(popup, WM_MOUSEMOVE, MK_LBUTTON, packCoords(ex - pr.left, sy - pr.top));
    }
    await client.waitForRenderIdle();
    // a stray system move (at the real cursor) can undo ours: drag there again
    const during = await pollUntil(
      () => {
        if (!USE_NG) {
          sendMessage(popup, WM_MOUSEMOVE, MK_LBUTTON, packCoords(ex - pr.left, sy - pr.top));
        }
        return inkWidth(client);
      },
      (w) => w === 1,
      { timeoutMs: 3000, error: (w) => `issue-6388: the stroke stayed ${w} points wide during the drag, want 1` },
    );
    if (USE_NG) {
      await ngInput(client, "up", ex, sy, 0);
    } else {
      sendMessage(popup, WM_LBUTTONUP, 0, packCoords(ex - pr.left, sy - pr.top));
    }
    if (during !== 1) {
      throw new Error(`issue-6388: the stroke is ${during} points wide during the drag, want 1`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6388: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
