// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6402
//
// Edge-printed PDFs use type 1 (function-based) shadings that are black outside
// the painted area. Sampling them on a coarse grid smeared that black across
// the edge of the area as a dark border.
//
// Run:  bun tests/issue-6402.ts [--no-build]   (or via tests/run-almost-all.ts)

import { readFileSync, writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const PAGE_SIZE = 200;
// shading domain (0,0)..(1,1) maps to this square, in points
const AREA_MIN = 50;
const AREA_MAX = 150;
const GRAY = 0.9;
// pixels darker than this inside the area come from the black outside it
const MIN_GRAY = Math.round(GRAY * 255) - 10;

// black for x <= 0 or y <= 0, gray elsewhere; the domain reaches far past 0
// so the grid spacing is coarse at the edge
function makeFixture(): string {
  const fn = `{2 copy 0 le exch 0 le or {pop pop 0 0 0}{pop pop ${GRAY} ${GRAY} ${GRAY}} ifelse}`;
  const size = AREA_MAX - AREA_MIN;
  const content = `/Pattern cs /P1 scn ${AREA_MIN} ${AREA_MIN} ${size} ${size} re f`;
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_SIZE} ${PAGE_SIZE}] /Resources << /Pattern << /P1 5 0 R >> >> /Contents 4 0 R >>`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /PatternType 2 /Matrix [${size} 0 0 ${size} ${AREA_MIN} ${AREA_MIN}] /Shading << /ShadingType 1 /ColorSpace /DeviceRGB /Domain [-4 1 -4 1] /Function 6 0 R >> >>`,
    `<< /FunctionType 4 /Domain [-4 1 -4 1] /Range [0 1 0 1 0 1] /Length ${fn.length} >>\nstream\n${fn}\nendstream`,
  ];

  let pdf = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objects.length; i++) {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xref = pdf.length;
  pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
  pdf += offsets.map((offset) => `${offset.toString().padStart(10, "0")} 00000 n \n`).join("");
  pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

// binary PGM (P5) as rows of gray bytes
function readPgm(path: string): { w: number; h: number; px: Uint8Array } {
  const b = readFileSync(path);
  const fields: string[] = [];
  let i = 0;
  while (fields.length < 4) {
    while (b[i] === 0x20 || b[i] === 0x0a) i++;
    let s = "";
    while (b[i] !== 0x20 && b[i] !== 0x0a) s += String.fromCharCode(b[i++]);
    fields.push(s);
  }
  i++;
  const w = Number(fields[1]);
  const h = Number(fields[2]);
  return { w, h, px: new Uint8Array(b.buffer, b.byteOffset + i, w * h) };
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6402.pdf");
  const pgm = tmpPath("issue-6402.pgm");
  writeFileSync(pdf, makeFixture(), "ascii");

  // 72 dpi: one pixel per point
  const r = Bun.spawnSync([EXE, "draw", "-r", "72", "-c", "gray", "-o", pgm, pdf, "1"]);
  if (r.exitCode !== 0) {
    throw new Error(`issue-6402: draw failed: exit ${r.exitCode}, ${r.stderr.toString()}`);
  }
  const { w, h, px } = readPgm(pgm);
  if (w !== PAGE_SIZE || h !== PAGE_SIZE) {
    throw new Error(`issue-6402: unexpected render size ${w}x${h}`);
  }

  // the area minus its antialiased one-pixel edge; PGM rows go top-down
  let darkest = 255;
  let where = "";
  for (let y = AREA_MIN + 1; y < AREA_MAX - 1; y++) {
    for (let x = AREA_MIN + 1; x < AREA_MAX - 1; x++) {
      const v = px[(PAGE_SIZE - 1 - y) * w + x];
      if (v < darkest) {
        darkest = v;
        where = `${x},${y}`;
      }
    }
  }
  if (darkest < MIN_GRAY) {
    throw new Error(`issue-6402: shading has a dark border: gray ${darkest} at ${where} pt, want >= ${MIN_GRAY}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
