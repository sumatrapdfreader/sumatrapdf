// Builds tests/issue-6348.pdf from tests/issue-6348-font.ttf.
//
// The font is TELXKC+CambriaMath extracted from bug-6348.pdf. Two copies
// share that file: one FontDescriptor has the file's Ascent 3117 / Descent
// -2464 (a 5.6em box), the other Ascent 800 / Descent -200. The page draws
// ".( " in each, then a Helvetica H.
//
// Regenerate: bun tests/issue-6348-make.ts

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { deflateSync } from "node:zlib";

const OUT = join(import.meta.dir, "issue-6348.pdf");
const TTF = join(import.meta.dir, "issue-6348-font.ttf");

function buildPdf(ttf: Buffer): Buffer {
  const ttfZ = deflateSync(ttf);
  // ". (" in the tall font, ". (" in the 1em font, then a Helvetica H.
  // Hex strings: a literal "(" would have to be escaped inside a PDF string.
  const content = "BT /F1 13 Tf 50 150 Td <2E2028> Tj /F2 13 Tf 50 110 Td <2E2028> Tj /F3 12 Tf 50 70 Td <48> Tj ET";

  const objs: string[] = [];
  objs.push("<< /Type /Catalog /Pages 2 0 R >>");
  objs.push("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  objs.push(
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 220] " +
      "/Resources << /Font << /F1 4 0 R /F2 5 0 R /F3 10 0 R >> >> /Contents 9 0 R >>",
  );
  objs.push(
    "<< /Type /Font /Subtype /TrueType /BaseFont /Tall+CambriaMath /Encoding /WinAnsiEncoding " +
      "/FirstChar 32 /LastChar 46 /Widths [220 0 0 0 0 0 0 0 415 0 0 0 0 0 205] " +
      "/FontDescriptor 6 0 R >>",
  );
  objs.push(
    "<< /Type /Font /Subtype /TrueType /BaseFont /Norm+CambriaMath /Encoding /WinAnsiEncoding " +
      "/FirstChar 32 /LastChar 46 /Widths [220 0 0 0 0 0 0 0 415 0 0 0 0 0 205] " +
      "/FontDescriptor 7 0 R >>",
  );
  objs.push(
    "<< /Type /FontDescriptor /FontName /Tall+CambriaMath /Flags 32 /ItalicAngle 0 /StemV 80 " +
      "/Ascent 3117 /Descent -2464 /CapHeight 667 /FontBBox [-1475 -2464 2868 3117] /FontFile2 8 0 R >>",
  );
  objs.push(
    "<< /Type /FontDescriptor /FontName /Norm+CambriaMath /Flags 32 /ItalicAngle 0 /StemV 80 " +
      "/Ascent 800 /Descent -200 /CapHeight 667 /FontBBox [-200 -200 1200 900] /FontFile2 8 0 R >>",
  );
  objs.push(`<< /Filter /FlateDecode /Length ${ttfZ.length} /Length1 ${ttf.length} >>\nstream\n`);
  objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  objs.push("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");

  let pdf = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    if (i === 7) {
      pdf += `${i + 1} 0 obj\n${objs[i]}`;
      pdf += ttfZ.toString("binary");
      pdf += "\nendstream\nendobj\n";
      continue;
    }
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }

  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${off.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

const ttf = readFileSync(TTF);
writeFileSync(OUT, buildPdf(ttf));
console.log(`wrote ${OUT} (${readFileSync(OUT).length} bytes)`);
