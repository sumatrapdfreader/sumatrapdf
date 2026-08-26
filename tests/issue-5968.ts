// #5968: search/copy dropped a letter from tiny adjacent identical glyphs
// ("IO224" → "IO24", "RGMII_…" → "RGMI_…"). HasSeenGlyph compared
// integer-rounded boxes; RectF::Round() expands outward, so ~2.5pt "II"/"22"
// shared most of a 1–2px box and the second letter was discarded. MuPDF itself
// extracted the strings correctly.
//
// Run: bun tests/issue-5968.ts [--no-build]

import { existsSync, writeFileSync } from "node:fs";
import { extractPageText, runStandalone, tmpPath, assemblePdf } from "./util.ts";

const kNeedles = ["IO224", "RGMII_BMC_RMM4_TXD0"];
const kRealPdf = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-5968-Find_Zoom_Issues.pdf";

function makeTinyPdf(): string {
  // 2.5pt matches the schematic labels that triggered #5968. Helvetica "I"
  // at that size is ~0.7pt wide; outward integer rounding used to merge
  // adjacent identical letters.
  const content = "BT /F1 2.5 Tf 72 720 Td (IO224 RGMII_BMC_RMM4_TXD0) Tj ET";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  return assemblePdf(objs);
}

function assertNeedles(text: string, label: string): void {
  // -extract-text encodes real newlines as '_'; extractPageText turns every
  // '_' into '\n', including underscores that are part of the net names.
  // Join back so "RGMII_BMC_…" is one token again.
  const joined = text.replace(/\n/g, "_");
  for (const needle of kNeedles) {
    if (!joined.includes(needle)) {
      throw new Error(`issue-5968: ${label} missing ${JSON.stringify(needle)}:\n${JSON.stringify(joined)}`);
    }
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5968.pdf");
  writeFileSync(pdf, makeTinyPdf(), "latin1");
  const tiny = extractPageText(pdf, 1);
  assertNeedles(tiny, "2.5pt Helvetica");
  console.log("  2.5pt Helvetica keeps IO224 and RGMII_BMC_RMM4_TXD0 ✓");

  if (existsSync(kRealPdf)) {
    const real = extractPageText(kRealPdf, 1);
    assertNeedles(real, "bug-5968 PDF");
    console.log("  schematic PDF keeps IO224 and RGMII_BMC_RMM4_TXD0 ✓");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
