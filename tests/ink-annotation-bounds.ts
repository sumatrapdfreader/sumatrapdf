// An ink annotation's bounds (hover mark, hit area, selection box) hug the
// stroke: the points' extent plus the line width. mupdf pads /Rect by
// width + 6pt on each side, which made the box much bigger than the ink.
//
// Run: bun tests/ink-annotation-bounds.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getClientRect,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, pressEscape, sendCommandSync } from "./win-automation.ts";

type Point = { x: number; y: number };
type RectF = { x: number; y: number; dx: number; dy: number };

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

async function dump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

function parseRect(m: RegExpExecArray): RectF {
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
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
  const dir = tmpPath("ink-annotation-bounds");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
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
    const cr = getClientRect(canvas);
    const cx = Math.floor(cr.right / 2);
    const cy = Math.floor(cr.bottom / 2);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await sleep(200);
    sendCommandSync(frame, cmdId("CmdCreateAnnotInk"));
    await sleep(200);
    await drawStroke(canvas, [
      { x: cx - 80, y: cy + 40 },
      { x: cx - 30, y: cy + 70 },
      { x: cx + 30, y: cy + 40 },
      { x: cx + 80, y: cy + 70 },
    ]);

    const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
    let raw = await dump(client);
    while (!/inkRect=/.test(raw) && Date.now() < deadline) {
      await sleep(40);
      raw = await dump(client);
    }
    const rm = /type=Ink page=\d+ rect=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(raw);
    const pm = /inkRect=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(raw);
    const wm = /ink strokes=\d+ points=\d+ opacity=\d+ width=(\d+)/.exec(raw);
    if (!rm || !pm || !wm) {
      throw new Error(`ink-annotation-bounds: ink stroke was not committed\n${raw}`);
    }
    const rect = parseRect(rm);
    const pts = parseRect(pm);
    const width = +wm[1]!;
    const want = { x: pts.x - width / 2, y: pts.y - width / 2, dx: pts.dx + width, dy: pts.dy + width };
    const off = Math.max(
      Math.abs(rect.x - want.x),
      Math.abs(rect.y - want.y),
      Math.abs(rect.dx - want.dx),
      Math.abs(rect.dy - want.dy),
    );
    if (off > 1) {
      throw new Error(
        `ink-annotation-bounds: bounds ${JSON.stringify(rect)}, want stroke + width ${JSON.stringify(want)}\n${raw}`,
      );
    }
    await pressEscape(frame);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("ink-annotation-bounds: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
