// #6136: a wide page makes the canvas wider than a later narrow page, which
// sits centered. Scrolling fully left on that narrow row puts the viewport in
// empty space (no page intersects it). DrawDocument then skipped flushing the
// double-buffer (gNoFlickerRender), so the previous page's pixels stayed on
// screen — and a file reload kept showing the old page until you scrolled.
//
// Run: bun tests/issue-6136.ts [--no-build]

import { writeFileSync } from "node:fs";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { ControlCommand } from "./control.ts";
import { captureWindowPixels, getClientRect, getScrollInfo, sendMessage, sleep, SB_HORZ } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const WM_HSCROLL = 0x0114;
const WM_VSCROLL = 0x0115;
const SB_LEFT = 6;
const SB_PAGERIGHT = 3;
const SB_PAGEDOWN = 3;

// Page 1 is much wider than any test window so page 2 (centered) sits far to
// the right. Page 2 is taller than the window so we can scroll page 1 off.
const PAGE1_W = 8000;
const PAGE1_H = 400;
const PAGE2_W = 400;
const PAGE2_H = 2400;

type PageScreen = { n: number; x: number; y: number; dx: number; dy: number };

function makePdf(page2Rgb: string): string {
  const p1 = `0.85 0.85 0.85 rg 0 0 ${PAGE1_W} ${PAGE1_H} re f`;
  const p2 = `${page2Rgb} rg 0 0 ${PAGE2_W} ${PAGE2_H} re f`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE1_W} ${PAGE1_H}] /Contents 5 0 R >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE2_W} ${PAGE2_H}] /Contents 6 0 R >>`,
    `<< /Length ${p1.length} >>\nstream\n${p1}\nendstream`,
    `<< /Length ${p2.length} >>\nstream\n${p2}\nendstream`,
  ]);
}

function countRedPx(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i + 3 < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r > 180 && g < 50 && b < 50) {
      n++;
    }
  }
  return n;
}

function countPainted(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i + 3 < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r + g + b > 30) {
      n++;
    }
  }
  return n;
}

async function layoutPages(client: { request: Function }): Promise<PageScreen[]> {
  const res = await client.request(ControlCommand.TestLayout, ["get"]);
  const raw = String(res[1] ?? "");
  const pages: PageScreen[] = [];
  const re = /page n=(\d+) shown=\d+ pos=[^\n]* screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  for (const m of raw.matchAll(re)) {
    pages.push({ n: +m[1]!, x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! });
  }
  return pages;
}

function intersectsCanvas(p: PageScreen, w: number, h: number): boolean {
  return p.x < w && p.x + p.dx > 0 && p.y < h && p.y + p.dy > 0;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6136.pdf");
  writeFileSync(pdf, makePdf("1 0 0"), "latin1");

  const { proc, client, frame } = await launchControlled(["-zoom", "100", "-page", "2", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6136: canvas not found");
    }
    const rc = getClientRect(canvas);
    const canvasW = rc.right - rc.left;
    const canvasH = rc.bottom - rc.top;

    for (let i = 0; i < 40; i++) {
      const pages = await layoutPages(client);
      const p1 = pages.find((p) => p.n === 1);
      if (p1 && p1.y + p1.dy <= 0) {
        break;
      }
      sendMessage(canvas, WM_VSCROLL, SB_PAGEDOWN, 0);
    }

    for (let i = 0; i < 80; i++) {
      const px = captureWindowPixels(canvas);
      if (px && countRedPx(px.data) > 100) {
        break;
      }
      sendMessage(canvas, WM_HSCROLL, SB_PAGERIGHT, 0);
      await sleep(40);
    }
    const vis = captureWindowPixels(canvas);
    const redVisible = vis ? countRedPx(vis.data) : 0;
    if (redVisible < 100) {
      throw new Error(`issue-6136: page 2 did not paint red (red=${redVisible})`);
    }

    for (let i = 0; i < 40; i++) {
      sendMessage(canvas, WM_HSCROLL, SB_LEFT, 0);
      if (getScrollInfo(canvas, SB_HORZ).pos <= 0) {
        break;
      }
    }
    await sleep(150);

    const afterLeft = await layoutPages(client);
    const visible = afterLeft.filter((p) => intersectsCanvas(p, canvasW, canvasH));
    if (visible.length > 0) {
      throw new Error(`issue-6136: expected empty viewport at x=0, still visible: ${JSON.stringify(visible)}`);
    }

    sendCommandSync(frame, cmdId("CmdReloadDocument"));
    await client.waitForRenderIdle();

    const after = captureWindowPixels(canvas);
    if (!after) {
      throw new Error("issue-6136: could not capture canvas after reload");
    }
    const paintedAfter = countPainted(after.data);
    const redAfter = countRedPx(after.data);
    if (paintedAfter < 1000) {
      throw new Error(`issue-6136: canvas was not flushed at x=0 after reload (painted=${paintedAfter})`);
    }
    if (redAfter > redVisible / 4) {
      throw new Error(`issue-6136: stale page 2 still on screen after reload at x=0 (red=${redAfter})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
