// Arrow keys nudge the selected annotation (Edit PDF): an arrow moves it by a
// pixel on screen, without changing its size.
//
// Run: bun tests/annot-nudge.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, postMessage, sleep, WM_KEYDOWN } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const VK_LEFT = 0x25;
const VK_RIGHT = 0x27;
const VK_DOWN = 0x28;

type Square = { x: number; y: number; dx: number; dy: number; selected: boolean };

async function square(client: ControlClient): Promise<Square> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`annot-nudge: no square\n${raw}`);
  }
  const selected = /state selected=1/.test(raw);
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]!, selected };
}

async function waitSquare(client: ControlClient, what: string, pred: (s: Square) => boolean): Promise<Square> {
  const deadline = Date.now() + 5000;
  for (;;) {
    const s = await square(client);
    if (pred(s)) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-nudge: ${what}: ${JSON.stringify(s)}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-nudge");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "square.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [1 0 0] /BS << /W 2 >> >>",
    ]),
    "latin1",
  );

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    let s = await square(client);
    await clickAt(canvas, s.x + Math.floor(s.dx / 2), s.y + Math.floor(s.dy / 2));
    s = await waitSquare(client, "click did not select the square", (q) => q.selected);

    // the first change regenerates the square's appearance, which can shift
    // its bounds by a unit: only the direction is checked here
    postMessage(canvas, WM_KEYDOWN, VK_RIGHT, 0);
    s = await waitSquare(client, "Right did not move it right", (q) => q.x > s.x);
    postMessage(canvas, WM_KEYDOWN, VK_DOWN, 0);
    // same size: moving must not grow it (setting the bounds as /Rect did, by the border)
    s = await waitSquare(client, "Down did not move it down", (q) => q.y > s.y && q.x === s.x);
    const size = { dx: s.dx, dy: s.dy };
    postMessage(canvas, WM_KEYDOWN, VK_LEFT, 0);
    s = await waitSquare(
      client,
      "Left did not move it left, same size",
      (q) => q.x < s.x && q.y === s.y && q.dx === size.dx && q.dy === size.dy,
    );

    // a burst of nudges re-renders the page once the keys pause: the red
    // border must end up drawn where the square now is, not where it was
    const kBurst = 30;
    for (let i = 0; i < kBurst; i++) {
      postMessage(canvas, WM_KEYDOWN, VK_RIGHT, 0);
    }
    const moved = await waitSquare(client, "the burst did not move it", (q) => q.x >= s.x + kBurst);
    const isRed = (cap: { w: number; data: Uint8Array }, x: number, y: number) => {
      const i = (y * cap.w + x) * 4;
      return cap.data[i + 2]! > 200 && cap.data[i + 1]! < 80 && cap.data[i]! < 80;
    };
    // the left border column, at mid-height; client coords
    const midY = moved.y + Math.floor(moved.dy / 2);
    const redAt = (x: number) => {
      const cap = captureWindowPixels(canvas)!;
      for (let dx = -2; dx <= 2; dx++) {
        if (isRed(cap, x + dx, midY)) {
          return true;
        }
      }
      return false;
    };
    const deadline = Date.now() + 5000;
    while (!(redAt(moved.x) && !redAt(s.x))) {
      if (Date.now() > deadline) {
        throw new Error(
          `annot-nudge: page not re-rendered after the burst: red at new ${redAt(moved.x)} old ${redAt(s.x)}`,
        );
      }
      await sleep(50);
    }
    console.log("annot-nudge: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
