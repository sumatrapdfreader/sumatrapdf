// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3271
//
// Free text in Persian, Arabic or Hebrew was laid out with a left-to-right base
// direction: "سلام اسم من ali هست" came out word-reversed around the Latin word
// and left-aligned. It must read right to left: سلام rightmost, then "ali",
// then هست, ending at the right edge of the annotation.
//
// The annotation has no appearance stream, so mupdf builds one; `sumatrapdf-tool
// draw -F stext` reports where each glyph landed.
//
// Run:  bun tests/issue-3271.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util.ts";

const TEXT = "سلام اسم من ali هست";
const RECT = { x0: 100, x1: 500 };
// how far from the right edge the last glyph may end
const kEdgeSlack = 20;

function makePdf(path: string): void {
  const utf16 = Buffer.from(String.fromCharCode(0xfeff) + TEXT, "utf16le")
    .swap16()
    .toString("hex");
  const annot =
    `<< /Type /Annot /Subtype /FreeText /Rect [${RECT.x0} 600 ${RECT.x1} 640] ` +
    `/Contents <${utf16}> /DA (/Helv 14 Tf 0 g) >>`;
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot}] >>`,
  ];
  writeFileSync(path, assemblePdf(objs));
}

type Glyph = { c: number; x0: number; x1: number };

function parseGlyphs(stext: string): Glyph[] {
  const out: Glyph[] = [];
  for (const m of stext.matchAll(/<char c="([^"]*)" quad="([^"]*)"/g)) {
    const raw = m[1]!;
    const hex = /^&#x([0-9a-f]+);$/i.exec(raw);
    const c = hex ? parseInt(hex[1]!, 16) : raw.codePointAt(0)!;
    const q = m[2]!.split(" ").map(Number);
    out.push({ c, x0: Math.min(q[0]!, q[4]!), x1: Math.max(q[2]!, q[6]!) });
  }
  return out;
}

function xOf(glyphs: Glyph[], c: number, what: string): number {
  const g = glyphs.find((g) => g.c === c);
  if (!g) {
    throw new Error(`#3271: no glyph for ${what} in the rendered annotation`);
  }
  return g.x0;
}

export async function testit(): Promise<void> {
  const tool = join(dirname(EXE), "sumatrapdf-tool.exe");
  if (!existsSync(tool)) {
    console.log("  skipping issue-3271: no sumatrapdf-tool.exe from the tested build");
    return;
  }
  const pdf = tmpPath("issue-3271.pdf");
  const stext = tmpPath("issue-3271.stext.xml");
  makePdf(pdf);
  const p = Bun.spawnSync([tool, "draw", "-F", "stext", "-o", stext, pdf]);
  if (p.exitCode !== 0 || !existsSync(stext)) {
    throw new Error(`#3271: sumatrapdf-tool draw failed (${p.exitCode}): ${p.stderr.toString()}`);
  }
  const glyphs = parseGlyphs(readFileSync(stext, "utf8"));
  const got = glyphs.map((g) => `${String.fromCodePoint(g.c)}@${g.x0.toFixed(0)}`).join(" ");

  // first letter of سلام, the Latin word, first letter of هست
  const xSalam = xOf(glyphs, 0x633, "س");
  const xAli = xOf(glyphs, "a".codePointAt(0)!, "a");
  const xHast = xOf(glyphs, 0x647, "ه");
  if (!(xSalam > xAli && xAli > xHast)) {
    throw new Error(`#3271: words should read right to left (سلام, ali, هست), got: ${got}`);
  }

  const right = Math.max(...glyphs.map((g) => g.x1));
  if (right < RECT.x1 - kEdgeSlack) {
    throw new Error(`#3271: right-to-left text should end at the right edge (${RECT.x1}), ends at ${right}: ${got}`);
  }
  console.log(`issue-3271: OK right=${right.toFixed(0)}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
