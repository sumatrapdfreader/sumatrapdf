// A pen's eraser end removes whole InkList strokes while the ink tool stays active.
// Finish/cancel via -dbg-control: a posted Enter does not reach the ink tool.

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
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Point = { x: number; y: number };

type InkState = {
  active: boolean;
  strokes: number;
  points: number;
  annotations: number;
  savedStrokes: number;
  savedPoints: number;
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
  const placement = /inkPlacement active=(\d+).* strokes=(\d+) points=(\d+)/.exec(raw);
  const annotations = /annotations=(\d+)/.exec(raw);
  const saved = /ink strokes=(\d+) points=(\d+)/.exec(raw);
  if (res[0] !== 0 || !placement || !annotations) {
    throw new Error(`issue-6135: could not read ink state\n${raw}`);
  }
  return {
    active: placement[1] === "1",
    strokes: +placement[2]!,
    points: +placement[3]!,
    annotations: +annotations[1]!,
    savedStrokes: +(saved?.[1] ?? 0),
    savedPoints: +(saved?.[2] ?? 0),
    raw,
  };
}

async function drawStroke(canvas: number, points: Point[]): Promise<void> {
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
  const last = points[points.length - 1]!;
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(last.x, last.y));
  await sleep(50);
}

async function eraseAt(client: ControlClient, point: Point): Promise<InkState> {
  return inkState(client, ["erase-ink", point.x, point.y]);
}

async function waitForInkActive(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  for (;;) {
    if ((await inkState(client)).active) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-6135: ink tool did not start");
    }
    await sleep(40);
  }
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
  throw new Error(`issue-6135: ${msg}\n${state.raw}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6135");
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
    const rect = getClientRect(canvas);
    const center = { x: Math.floor(rect.right / 2), y: Math.floor(rect.bottom / 2) };
    const first = [
      { x: center.x - 120, y: center.y - 50 },
      { x: center.x - 80, y: center.y - 50 },
      { x: center.x - 40, y: center.y - 50 },
    ];
    const second = [
      { x: center.x + 40, y: center.y + 50 },
      { x: center.x + 80, y: center.y + 50 },
      { x: center.x + 120, y: center.y + 50 },
    ];

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    sendCommand(frame, cmdId("CmdCreateAnnotInk"));
    await waitForInkActive(client);
    await drawStroke(canvas, first);
    await drawStroke(canvas, second);

    let state = await waitUntil(client, (s) => s.active && s.annotations === 2, "could not commit two ink strokes");

    state = await eraseAt(client, first[1]!);
    if (!state.active || state.annotations !== 1) {
      throw new Error(`issue-6135: eraser did not remove one committed stroke\n${state.raw}`);
    }

    state = await eraseAt(client, second[1]!);
    if (!state.active || state.annotations !== 0) {
      throw new Error(`issue-6135: eraser did not delete the remaining ink\n${state.raw}`);
    }
    await inkState(client, ["cancel-ink", 0, 0]);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6135: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
