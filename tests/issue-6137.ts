// #6137: highlighter stays 40% while dragging and after commit, finishes on
// mouse/pen up, and closing the placement hint finishes ink (touch, no Enter).
//
// Run: bun tests/issue-6137.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  captureWindowPixels,
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
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Point = { x: number; y: number };

type InkState = {
  active: boolean;
  strokes: number;
  annotations: number;
  opacity: number;
  raw: string;
};

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

async function inkState(client: ControlClient, args: (string | number)[] = []): Promise<InkState> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, args);
  const raw = String(res[1] ?? "");
  const placement = /inkPlacement active=(\d+).* strokes=(\d+)/.exec(raw);
  const annotations = /annotations=(\d+)/.exec(raw);
  const opacity = /ink strokes=\d+ points=\d+ opacity=(\d+)/.exec(raw);
  if (res[0] !== 0 || !placement || !annotations) {
    throw new Error(`issue-6137: could not read ink state\n${raw}`);
  }
  return {
    active: placement[1] === "1",
    strokes: +placement[2]!,
    annotations: +annotations[1]!,
    opacity: opacity ? +opacity[1]! : -1,
    raw,
  };
}

function densify(points: Point[], step = 4): Point[] {
  const out: Point[] = [points[0]!];
  for (let i = 1; i < points.length; i++) {
    const a = points[i - 1]!;
    const b = points[i]!;
    const dx = b.x - a.x;
    const dy = b.y - a.y;
    const n = Math.max(1, Math.floor(Math.hypot(dx, dy) / step));
    for (let k = 1; k <= n; k++) {
      out.push({ x: Math.round(a.x + (dx * k) / n), y: Math.round(a.y + (dy * k) / n) });
    }
  }
  return out;
}

async function pressStroke(canvas: number, points: Point[]): Promise<void> {
  const first = points[0]!;
  const screen = clientToScreen(canvas, first.x, first.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(first.x, first.y));
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(first.x, first.y));
  await sleep(50);
  for (let i = 1; i < points.length; i++) {
    const point = points[i]!;
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(point.x, point.y));
  }
}

function releaseStroke(canvas: number, point: Point): void {
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(point.x, point.y));
}

async function drawStroke(canvas: number, points: Point[]): Promise<void> {
  await pressStroke(canvas, points);
  releaseStroke(canvas, points[points.length - 1]!);
  await sleep(50);
}

// 40% yellow over white is B≈153. Per-segment DrawLine stacks round caps, so
// vertices (and a dense stroke) drop toward B≈92 / 55. captureWindowPixels is BGRA.
function yellowMinBlue(px: { data: Uint8Array } | null): { n: number; minB: number } {
  let n = 0;
  let minB = 255;
  if (!px) {
    return { n, minB };
  }
  for (let i = 0; i < px.data.length; i += 4) {
    const b = px.data[i]!;
    const g = px.data[i + 1]!;
    const r = px.data[i + 2]!;
    if (r > 200 && g > 200 && b < 200 && r - b > 40 && g - b > 40) {
      n++;
      if (b < minB) {
        minB = b;
      }
    }
  }
  return { n, minB };
}

async function waitUntil(client: ControlClient, pred: (s: InkState) => boolean, msg: string): Promise<InkState> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let state = await inkState(client);
  while (Date.now() < deadline) {
    if (pred(state)) {
      return state;
    }
    await sleep(40);
    state = await inkState(client);
  }
  throw new Error(`issue-6137: ${msg}\n${state.raw}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6137");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nCheckForUpdates = false\n",
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
    const canvasRect = getClientRect(canvas);
    const center = { x: Math.floor(canvasRect.right / 2), y: Math.floor(canvasRect.bottom / 2) };
    const stroke = [
      { x: center.x - 80, y: center.y - 20 },
      { x: center.x - 40, y: center.y + 10 },
      { x: center.x, y: center.y - 15 },
      { x: center.x + 40, y: center.y + 20 },
    ];

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(200);

    sendCommand(frame, cmdId("CmdAnnotationHighlightBrush"));
    await waitUntil(client, (s) => s.active, "highlighter did not start");
    await client.setNotificationsEnabled(false);

    const previewStroke = densify(stroke);
    await pressStroke(canvas, previewStroke);
    await waitUntil(client, (s) => s.active && s.strokes >= 1, "highlighter drag did not record a stroke");
    let preview = { n: 0, minB: 255 };
    const previewDeadline = Date.now() + 2000 * SLOW_BUILD_FACTOR;
    while (Date.now() < previewDeadline) {
      preview = yellowMinBlue(captureWindowPixels(canvas));
      if (preview.n >= 200) {
        break;
      }
      await sleep(40);
    }
    releaseStroke(canvas, previewStroke[previewStroke.length - 1]!);
    // stacked per-segment caps drop B toward 90; a single 40% stroke stays ~153
    if (preview.n < 200 || preview.minB < 120) {
      throw new Error(
        `issue-6137: highlighter drag preview should stay ~40% (B≳120), got n=${preview.n} minB=${preview.minB}`,
      );
    }

    await client.setNotificationsEnabled(true);

    let state = await waitUntil(
      client,
      (s) => !s.active && s.annotations === 1,
      "highlighter did not commit on mouse up",
    );
    // 40% of 255
    if (state.opacity < 90 || state.opacity > 120) {
      throw new Error(`issue-6137: highlighter opacity should be ~40% (102), got ${state.opacity}\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await waitUntil(client, (s) => s.active, "ink tool did not start");
    await drawStroke(canvas, stroke);
    await inkState(client, ["close-placement-hint", 0, 0]);
    state = await waitUntil(
      client,
      (s) => !s.active && s.annotations === 2,
      "closing the placement hint did not finish ink",
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
