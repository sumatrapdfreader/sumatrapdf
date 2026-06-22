// Builds tests/issue-3219.pdf: a one-page PDF with the word "Embedded" in an
// MSTT-style embedded Type1 subset font (glyph names G<hex>, no ToUnicode).
//
// The font program is tests/issue-3219-font.cff (extracted once from the
// original GSM fixture with: mutool extract tests/issue-3219.pdf).
//
// Regenerate: bun tests/issue-3219-make.ts

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { deflateSync } from "node:zlib";

const OUT = join(import.meta.dir, "issue-3219.pdf");
const CFF = join(import.meta.dir, "issue-3219-font.cff");

const TEXT = "Embedded";

function buildPdf(cff: Buffer): Buffer {
  const cffZ = deflateSync(cff);
  const content = `BT /F1 24 Tf 72 720 Td (${TEXT}) Tj ET`;

  const firstChar = 32;
  const lastChar = 126;
  const widths = Array.from({ length: lastChar - firstChar + 1 }, () => 602).join(" ");

  const objs: string[] = [];
  objs.push("<< /Type /Catalog /Pages 2 0 R >>");
  objs.push("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  objs.push(
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] " +
      "/Resources << /Font << /F1 4 0 R >> >> /Contents 8 0 R >>",
  );
  objs.push(
    "<< /Type /Font /Subtype /Type1 /BaseFont /SUBSET+MSTT31c39b " +
      `/FirstChar ${firstChar} /LastChar ${lastChar} ` +
      `/Widths [${widths}] ` +
      "/Encoding 5 0 R /FontDescriptor 6 0 R >>",
  );
  objs.push("<< /Type /Encoding /Differences [ 127 /G7F 128 /G80 ] >>");
  objs.push(
    "<< /Type /FontDescriptor /Ascent 0 /CapHeight 0 /Descent 0 /Flags 4 " +
      "/FontBBox [-12 -274 615 768] /FontName /SUBSET+MSTT31c39b /ItalicAngle 0 /StemV 0 " +
      "/CharSet (/G45/G62/G64/G65/G6D/G72) /FontFile3 7 0 R >>",
  );
  objs.push(
    `<< /Filter /FlateDecode /Length ${cffZ.length} /Subtype /Type1C >>\nstream\n`,
  );
  objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);

  let pdf = "%PDF-1.3\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    if (i === 6) {
      pdf += `${i + 1} 0 obj\n${objs[i]}`;
      pdf += cffZ.toString("binary");
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

const cff = readFileSync(CFF);
writeFileSync(OUT, buildPdf(cff));
console.log(`wrote ${OUT} (${readFileSync(OUT).length} bytes)`);