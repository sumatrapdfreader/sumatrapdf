// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6332
//
// MuPDF ignored <textPath>, so text on a path was drawn at the origin.
// A horizontal path at y=200 must put the text just above that baseline,
// and a vertical path must stand the text up along it.
//
// Run: bun tests/issue-6332.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const PAGE_W = 400;
const PAGE_H = 300;

function svg(pathD: string, text: string): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"` +
    ` width="${PAGE_W}" height="${PAGE_H}" viewBox="0 0 ${PAGE_W} ${PAGE_H}">` +
    `<path id="p" d="${pathD}" fill="none" stroke="none"/>` +
    `<text font-family="Helvetica" font-size="30" fill="#000">` +
    `<textPath href="#p">${text}</textPath></text></svg>`
  );
}

type Box = { x0: number; y0: number; x1: number; y1: number; nonWhite: number; raw: string };

async function renderBox(client: ControlClient, path: string): Promise<Box> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [path, 1, 100]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6332: render failed: ${raw}`);
  }
  const b = (/box=([\d,]+)/.exec(raw)?.[1] ?? "0,0,0,0").split(",").map(Number);
  return {
    x0: b[0]!,
    y0: b[1]!,
    x1: b[2]!,
    y1: b[3]!,
    nonWhite: Number(/nonwhite=(\d+)/.exec(raw)?.[1] ?? 0),
    raw,
  };
}

function expectBox(what: string, b: Box, want: { x0: number; y0: number; x1: number; y1: number }) {
  console.log(`issue-6332: ${what} box=${b.x0},${b.y0},${b.x1},${b.y1} nonwhite=${b.nonWhite}`);
  if (b.nonWhite < 50) {
    throw new Error(`issue-6332: ${what}: the text did not paint: ${b.raw}`);
  }
  const inside = b.x0 >= want.x0 && b.y0 >= want.y0 && b.x1 <= want.x1 && b.y1 <= want.y1;
  if (!inside) {
    throw new Error(`issue-6332: ${what}: text outside ${JSON.stringify(want)}: ${b.raw}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6332");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const horizontal = join(dir, "horizontal.svg");
  const vertical = join(dir, "vertical.svg");
  writeFileSync(horizontal, svg("M100,200 L350,200", "HELLO WORLD"));
  writeFileSync(vertical, svg("M50,50 L50,250", "DOWN"));

  await withControlledSumatra(EXE, async (client) => {
    // glyphs sit on the baseline y=200, starting at x=100
    expectBox("horizontal", await renderBox(client, horizontal), { x0: 95, y0: 165, x1: 355, y1: 205 });
    // the baseline runs down x=50; rotated clockwise, the glyph tops face right
    expectBox("vertical", await renderBox(client, vertical), { x0: 45, y0: 45, x1: 85, y1: 180 });
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
