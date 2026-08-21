// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4157
//
// Some PDFs put `0 Tc` / `0 Tw` inside a TJ array. MuPDF already applied those
// operators, then fell through to a syntax error. After 100 such errors it
// drops the rest of the page, so later text never draws (SM4007 datasheet).
//
// This fixture repeats that broken TJ more than 100 times, then draws MARKER.
// Before the fix, -extract-text has no MARKER. The same bytes live in
// tests/issue-4157.pdf (for an upstream MuPDF report); regenerate by writing
// makePdf() there.
//
// Run: bun tests/issue-4157.ts [--no-build]

import { writeFileSync } from "node:fs";
import { extractPageText, runStandalone, tmpPath } from "./util.ts";

export function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const ops: string[] = ["BT /F1 12 Tf 72 720 Td"];
  for (let i = 0; i < 110; i++) {
    ops.push("[(x) 0 Tc -10 (y) ] TJ");
  }
  ops.push("(MARKER) Tj");
  ops.push("ET");
  const stream = ops.join("\n");
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>"),
    5: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
  };
  const parts: Buffer[] = [enc("%PDF-1.4\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= 5; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 6\n0000000000 65535 f \n`;
  for (let n = 1; n <= 5; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-4157.pdf");
  writeFileSync(pdf, makePdf());
  const text = extractPageText(pdf, 1);
  if (!text.includes("MARKER")) {
    throw new Error(`issue-4157: expected MARKER after 110 broken TJ arrays, got ${JSON.stringify(text)}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
