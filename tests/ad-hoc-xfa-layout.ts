// Ad-hoc: verify XFA table/row column layout on IRS f1040 dependents grid.
//
// Run: bun tests/ad-hoc-xfa-layout.ts [--no-build]

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-f1040.pdf");

type FieldRect = { x0: number; y0: number; x1: number; y1: number };

function mmToPt(mm: number): number {
  return (mm * 72) / 25.4;
}

function parseProbeOutput(raw: string): Map<string, FieldRect> {
  const map = new Map<string, FieldRect>();
  for (const line of raw.split(/\r?\n/)) {
    const m = line.match(/^([^=]+)=([-\d.]+),([-\d.]+),([-\d.]+),([-\d.]+)$/);
    if (!m) continue;
    map.set(m[1], {
      x0: Number(m[2]),
      y0: Number(m[3]),
      x1: Number(m[4]),
      y1: Number(m[5]),
    });
  }
  return map;
}

function assertRect(name: string, rect: FieldRect | undefined): FieldRect {
  if (!rect) throw new Error(`missing probe rect for ${name}`);
  if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) {
    throw new Error(`${name}: empty rect ${JSON.stringify(rect)}`);
  }
  return rect;
}

export async function testit(): Promise<void> {
  const [code, raw] = await runControlCommand(EXE, ControlCommand.TestXfaFieldRects, [PDF, 1]);
  if (code !== 0) {
    throw new Error(`TestXfaFieldRects failed: ${(raw ?? "").trim()}`);
  }

  const rects = parseProbeOutput(String(raw ?? ""));
  const f14 = assertRect("f1_14", rects.get("f1_14"));
  const f15 = assertRect("f1_15", rects.get("f1_15"));
  const f16 = assertRect("f1_16", rects.get("f1_16"));
  const c12 = assertRect("c1_12", rects.get("c1_12"));
  const c13 = assertRect("c1_13", rects.get("c1_13"));

  if (!(f14.x0 < f15.x0 && f15.x0 < f16.x0 && f16.x0 < c12.x0 && c12.x0 < c13.x0)) {
    throw new Error(
      `dependents row1 x-order wrong: f1_14.x0=${f14.x0} f1_15.x0=${f15.x0} f1_16.x0=${f16.x0} c1_12.x0=${c12.x0} c1_13.x0=${c13.x0}`,
    );
  }

  const col0 = mmToPt(73.66);
  const col1 = mmToPt(30.48);
  const tol = 2.0;
  const w14 = f14.x1 - f14.x0;
  const gap15 = f15.x0 - f14.x0;
  const gap16 = f16.x0 - f15.x0;

  if (Math.abs(w14 - col0) > tol) {
    throw new Error(`f1_14 width ${w14.toFixed(2)} expected ~${col0.toFixed(2)} (col0)`);
  }
  if (Math.abs(gap15 - col0) > tol) {
    throw new Error(`f1_15 offset ${gap15.toFixed(2)} expected ~${col0.toFixed(2)} (col0)`);
  }
  if (Math.abs(gap16 - col1) > tol) {
    throw new Error(`f1_16 offset ${gap16.toFixed(2)} expected ~${col1.toFixed(2)} (col1)`);
  }

  const rowY = Math.abs(f14.y0 - f15.y0);
  if (rowY > 1.0) {
    throw new Error(`f1_14/f1_15 y0 mismatch: ${f14.y0} vs ${f15.y0}`);
  }

  // Row2 should sit below Row1.
  const f17 = assertRect("f1_17", rects.get("f1_17"));
  if (f17.y0 >= f14.y0) {
    throw new Error(`row2 f1_17.y0=${f17.y0} should be below row1 f1_14.y0=${f14.y0}`);
  }

  console.log(
    `dependents layout OK: row1 widths=${w14.toFixed(1)}/${(f15.x1 - f15.x0).toFixed(1)} gaps=${gap15.toFixed(1)}/${gap16.toFixed(1)}`,
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}