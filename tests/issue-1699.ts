// Test for issue #1699: "zoom to selection".
//
// With something selected, `Zoom / To Selection` (Ctrl + 4, also in the canvas
// context menu) zooms so the selection fills the window and centres it. Back
// (Alt + Left) returns to the view it was zoomed from, and the selection is
// left alone so it can still be copied.
//
// The test document is four solid colour quadrants with a line of text inside
// the top-left (red) one, so both the input and the result can be read off the
// pixels: after zooming into the selected text the window must be all red,
// with the other three quadrants scrolled out of view.
//
// It selects text by dragging (a rectangular Ctrl + drag selection can't be
// driven from a test: the app reads the real Ctrl key state, and injected
// keyboard state doesn't stick on the test machine).

import { writeFileSync } from "node:fs";
import { cmdId, tmpPath, assemblePdf } from "./util";
import { findCanvas, launchControlled, killAndWait } from "./win-automation";
import {
  captureWindowPixels,
  packCoords,
  postMessage,
  sendMessage,
  sleep,
  MK_LBUTTON,
  WM_COMMAND,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi";

// page coordinates of the text line, so the test can find it on screen
const TEXT_X = 40;
const TEXT_Y = 690;
const TEXT = "ZoomHereZoomHere";
const PAGE_DX = 612;
const PAGE_DY = 792;

// one page: four solid quadrants (red, green, blue, yellow) and a line of text
// in the red one
function makeQuadrantPdf(): string {
  const stream =
    [
      "1 0 0 rg 0 396 306 396 re f", // top-left red
      "0 0.6 0 rg 306 396 306 396 re f", // top-right green
      "0 0 1 rg 0 0 306 396 re f", // bottom-left blue
      "1 1 0 rg 306 0 306 396 re f", // bottom-right yellow
    ].join("\n") + `\nBT /F1 16 Tf 0 0 0 rg ${TEXT_X} ${TEXT_Y} Td (${TEXT}) Tj ET`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_DX} ${PAGE_DY}] /Contents 4 0 R ` +
      `/Resources << /Font << /F1 5 0 R >> >> >>`,
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>`,
  ];
  return assemblePdf(objs);
}

type Counts = { red: number; green: number; blue: number; yellow: number; total: number };

function classify(data: Uint8Array, i: number): keyof Counts | null {
  const b = data[i]!;
  const g = data[i + 1]!;
  const r = data[i + 2]!;
  if (r > 150 && g < 100 && b < 100) return "red";
  if (g > 100 && r < 100 && b < 100) return "green";
  if (b > 150 && r < 100 && g < 100) return "blue";
  if (r > 150 && g > 150 && b < 100) return "yellow";
  return null;
}

function colorCounts(canvas: number): Counts {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("could not capture the canvas");
  }
  const { data } = cap;
  const res: Counts = { red: 0, green: 0, blue: 0, yellow: 0, total: data.length / 4 };
  for (let i = 0; i < data.length; i += 4) {
    const k = classify(data, i);
    if (k) {
      res[k]++;
    }
  }
  return res;
}

// Bounding box of the text line on screen, found in the rendering itself so it
// needs no coordinate math: the dark pixels inside the red quadrant.
function textRectOnScreen(canvas: number): { x: number; y: number; dx: number; dy: number } {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("could not capture the canvas");
  }
  const { w, h, data } = cap;
  const isRed = (x: number, y: number) => classify(data, (y * w + x) * 4) === "red";
  let rx0 = w;
  let ry0 = h;
  let rx1 = -1;
  let ry1 = -1;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      if (!isRed(x, y)) {
        continue;
      }
      if (x < rx0) rx0 = x;
      if (y < ry0) ry0 = y;
      if (x > rx1) rx1 = x;
      if (y > ry1) ry1 = y;
    }
  }
  if (rx1 < 0) {
    throw new Error("the page doesn't seem to be rendered (no red quadrant)");
  }
  let x0 = rx1;
  let y0 = ry1;
  let x1 = -1;
  let y1 = -1;
  for (let y = ry0 + 2; y <= ry1 - 2; y++) {
    for (let x = rx0 + 2; x <= rx1 - 2; x++) {
      const i = (y * w + x) * 4;
      if (data[i]! > 90 || data[i + 1]! > 90 || data[i + 2]! > 90) {
        continue;
      }
      if (x < x0) x0 = x;
      if (y < y0) y0 = y;
      if (x > x1) x1 = x;
      if (y > y1) y1 = y;
    }
  }
  if (x1 < 0) {
    throw new Error("could not find the text line inside the red quadrant");
  }
  return { x: x0, y: y0, dx: x1 - x0 + 1, dy: y1 - y0 + 1 };
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
  await sleep(500);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-1699.pdf");
  writeFileSync(pdf, makeQuadrantPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("could not find the canvas window");
    }
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdZoomFitPage")), 0n);
    await client.waitForRenderIdle();

    const before = colorCounts(canvas);
    if (before.red < 1000 || before.blue < 1000) {
      throw new Error(`the whole page should be visible at fit page, got ${JSON.stringify(before)}`);
    }

    // Select the text line, which sits in the red quadrant. Done with the
    // keyboard selection commands: a mouse drag can't be synthesized reliably
    // here, and Select All selects the whole page rather than the text.
    const tr = textRectOnScreen(canvas);
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdSelectTextViaKeyboard")), 0n);
    for (let i = 0; i < 3; i++) {
      sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdExtendSelectionWordRight")), 0n);
    }

    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdZoomToSelection")), 0n);
    await client.waitForRenderIdle();

    const after = colorCounts(canvas);
    if (after.red < after.total * 0.5) {
      throw new Error(
        `Zoom To Selection did not zoom into the selected area: ` +
          `${JSON.stringify(after)} (before: ${JSON.stringify(before)})`,
      );
    }
    if (after.blue > before.blue / 4 || after.yellow > before.yellow / 4) {
      throw new Error(`the far quadrants should be off-screen after zooming: ${JSON.stringify(after)}`);
    }

    // Back returns to the view it was zoomed from
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdNavigateBack")), 0n);
    await client.waitForRenderIdle();
    const back = colorCounts(canvas);
    if (back.blue < before.blue / 2 || back.yellow < before.yellow / 2) {
      throw new Error(
        `Back did not return to the view before Zoom To Selection: ` +
          `${JSON.stringify(back)} (before: ${JSON.stringify(before)})`,
      );
    }
    console.log(`  zoom to selection: red ${before.red} -> ${after.red} px, Back restored the page ✓`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
