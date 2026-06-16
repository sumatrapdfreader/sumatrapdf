// Test for issue #933: case-insensitive search treats German ß as equivalent
// to "ss" (and vice versa).
//
// Builds a tiny PDF whose text contains ONLY one spelling of each word
// ("Straße"/"Fußball" with ß, "Klasse" with ss) and then searches for the
// OTHER spelling via the headless -test-search flag. Each needle's literal
// spelling is absent from the PDF, so a hit can only come from the ß<->ss
// equivalence — reverting the fix turns the FOUND assertions into NOTFOUND.
//
// Run:  bun tests/issue-933.ts [--no-build]

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// Build a minimal single-page PDF that shows the given lines using Helvetica
// with WinAnsiEncoding (so byte 0xDF renders/extracts as ß). `lines` are raw
// PDF literal-string bytes (use \337 for ß).
function buildPdf(lines: string[]): Buffer {
  const content =
    "BT /F1 24 Tf 72 720 Td " + lines.map((l, i) => (i === 0 ? `(${l}) Tj` : `0 -40 Td (${l}) Tj`)).join(" ") + " ET";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function search(pdfPath: string, needle: string): string {
  const outPath = tmpPath("issue-933-result.txt");
  const p = Bun.spawnSync({
    cmd: [EXE, "-for-testing", "-test-search", pdfPath, needle, outPath],
    stdout: "pipe",
    stderr: "pipe",
  });
  const out = existsSync(outPath) ? readFileSync(outPath, "utf-8") : p.stdout.toString();
  return out.trim();
}

export async function testit(): Promise<void> {
  // ß = octal \337 (0xDF in WinAnsi). PDF text has only these spellings.
  const pdfPath = tmpPath("issue-933.pdf");
  writeFileSync(pdfPath, buildPdf(["Stra\\337e", "Fu\\337ball", "Klasse"]));

  // needle -> expected result. The literal needle never appears in the PDF;
  // a FOUND can only come from ß<->ss folding.
  const cases: { needle: string; expectFound: boolean }[] = [
    { needle: "Strasse", expectFound: true }, // "ss" needle matches ß in "Straße"
    { needle: "Fussball", expectFound: true }, // "ss" needle matches ß in "Fußball"
    { needle: "Klaße", expectFound: true }, // "ß" needle matches "ss" in "Klasse"
    { needle: "Xyzzy", expectFound: false }, // control: no match
  ];

  for (const c of cases) {
    const res = search(pdfPath, c.needle);
    const found = res.startsWith("FOUND");
    console.log(`  search "${c.needle}" -> ${res}`);
    if (found !== c.expectFound) {
      throw new Error(`search "${c.needle}": expected ${c.expectFound ? "FOUND" : "NOTFOUND"}, got: ${res}`);
    }
  }

  console.log("✅ ß <-> ss search equivalence works");
}

if (import.meta.main) {
  await runStandalone(testit);
}
