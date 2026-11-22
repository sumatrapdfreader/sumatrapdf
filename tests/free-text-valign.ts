// A free text whose /DS says vertical-align:middle or bottom draws its text in
// the middle or at the bottom of its box (SumatraPDF's text box Vertical
// Alignment, stored where its bold / italic / underline are).
//
// Run: bun tests/free-text-valign.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, loadPng, runStandalone, tmpPath } from "./util.ts";

const name = "free-text-valign";
// three boxes side by side, 100pt tall, from y=600 (PDF space) up
const kBoxes = [
  { x: 50, valign: "" },
  { x: 220, valign: ";vertical-align:middle" },
  { x: 390, valign: ";vertical-align:bottom" },
];
const kBoxW = 150;
const kBoxY0 = 600;
const kBoxH = 100;

function pdf(): string {
  const annots = kBoxes.map(
    (b) =>
      `<< /Type /Annot /Subtype /FreeText /Rect [${b.x} ${kBoxY0} ${b.x + kBoxW} ${kBoxY0 + kBoxH}] ` +
      `/Contents (Hello) /DA (/Helv 12 Tf 0 g) /DS (font-family:Helvetica;font-size:12pt;color:#000000${b.valign}) /F 4 >>`,
  );
  const content = "q Q";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> /Annots [5 0 R 6 0 R 7 0 R] /Contents 4 0 R >>`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    ...annots,
  ];
  let s = "%PDF-1.7\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(s.length);
    s += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = s.length;
  s += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    s += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  s += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return s;
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const file = join(dir, "valign.pdf");
  writeFileSync(file, pdf(), "latin1");
  const png = join(dir, "valign.png");
  const p = Bun.spawnSync([EXE, "cli", "render", file, "--dpi", "72", "-o", png]);
  if (p.exitCode !== 0) {
    throw new Error(`${name}: render failed: ${p.stderr.toString()}`);
  }
  const img = loadPng(png);
  // the middle of each box's ink, in points from the top of its box
  const boxTop = 792 - (kBoxY0 + kBoxH);
  const mids = kBoxes.map((b) => {
    let y0 = -1;
    let y1 = -1;
    for (let y = boxTop + 2; y < boxTop + kBoxH - 2; y++) {
      for (let x = b.x + 2; x < b.x + kBoxW - 2; x++) {
        if (img.data[(y * img.w + x) * img.nComp]! < 128) {
          if (y0 < 0) {
            y0 = y;
          }
          y1 = y;
        }
      }
    }
    if (y0 < 0) {
      throw new Error(`${name}: no text in the box at x=${b.x}`);
    }
    return (y0 + y1) / 2 - boxTop;
  });
  const [top, middle, bottom] = mids as [number, number, number];
  if (!(top < 25 && Math.abs(middle - kBoxH / 2) < 10 && bottom > kBoxH - 25)) {
    throw new Error(`${name}: text centers ${JSON.stringify(mids)} in ${kBoxH}pt boxes, want top / middle / bottom`);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
