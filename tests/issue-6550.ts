// #6550: Ctrl+click on an annotation turns on Edit PDF. That shows the Edit
// PDF toolbar row and re-lays out the page, and the same press went on to
// drag the annotation with the old press point, so it moved. It must only
// be selected.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control";
import { ControlCommand } from "./control";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util";
import {
  clientToScreen,
  MK_CONTROL,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi";
import { clickAt, findCanvas, killAndWait, launchControlled } from "./win-automation";

const kRect = "72 420 192 540";

type State = { raw: string; rect: string; selected: boolean; editToolbar: boolean };

async function state(client: ControlClient): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const sq = /type=Square[^\n]*?rect=(\S+)/.exec(raw);
    const st = /state selected=(\d) hover=\d editToolbar=(\d)/.exec(raw);
    if (res[0] === 0 && sq && st) {
      return { raw, rect: sq[1]!, selected: st[1] === "1", editToolbar: st[2] === "1" };
    }
    if (Date.now() > deadline) throw new Error(`issue-6550: can't read the annotation state\n${raw}`);
    await sleep(50);
  }
}

function squareCenter(raw: string): { x: number; y: number } {
  const m = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (!m) throw new Error(`issue-6550: no square on screen\n${raw}`);
  return { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + Math.floor(+m[4]! / 2) };
}

// Ctrl+click as a person does it: the button comes up after the press has
// been handled and the window re-laid out, at the same place on screen
async function ctrlClick(client: ControlClient, frame: number, x: number, y: number): Promise<void> {
  const canvas = findCanvas(frame);
  // ng takes one click as a whole, after which its layout runs
  if (USE_NG) {
    await clickAt(canvas, x, y, 0, MK_CONTROL);
    return;
  }
  const screen = clientToScreen(canvas, x, y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON | MK_CONTROL, packCoords(x, y));
  await client.waitForRenderIdle();
  await sleep(200);
  // the canvas may have moved: the same screen point in its client coordinates
  const origin = clientToScreen(canvas, 0, 0);
  const lp = packCoords(screen.x - origin.x, screen.y - origin.y);
  sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON | MK_CONTROL, lp);
  sendMessage(canvas, WM_LBUTTONUP, MK_CONTROL, lp);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6550");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "square.pdf");
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      `<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [${kRect}] /C [1 0 0] /IC [1 1 0.6] /BS << /W 2 >> >>`,
    ]),
    "latin1",
  );
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const before = await state(client);
    if (before.editToolbar) throw new Error(`issue-6550: Edit PDF is on at start\n${before.raw}`);

    const c = squareCenter(before.raw);
    await ctrlClick(client, frame, c.x, c.y);
    await client.waitForRenderIdle();
    await sleep(200);
    const after = await state(client);
    if (!after.editToolbar || !after.selected) {
      throw new Error(`issue-6550: Ctrl+click did not select in Edit PDF\n${after.raw}`);
    }
    if (after.rect !== before.rect) {
      throw new Error(`issue-6550: Ctrl+click moved the annotation: ${before.rect} -> ${after.rect}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
