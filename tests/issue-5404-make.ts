// Builds tests/issue-5404.pdf: a minimal one-page PDF with an AcroForm text
// field whose value contains Central European Latin diacritics ("POČÍTAČ MODRÝ
// KAMEŇ"). The form has /NeedAppearances true and the widget has NO appearance
// stream, so a viewer must synthesise the field appearance from the field value
// (/V, stored as UTF-16BE).
//
// Before the fix, mupdf's appearance synthesis routed any text with characters
// outside WinAnsi/Greek/Cyrillic/CJK (i.e. Č, Ň, Ý, ...) through the rich/HTML
// layout path, which rendered the field blank. The fix adds a CP-1250 (Latin-2)
// path so these render normally. See tests/issue-5404.ts.
//
// Regenerate: bun tests/issue-5404-make.ts

import { writeFileSync } from "node:fs";
import { join } from "node:path";

const OUT = join(import.meta.dir, "issue-5404.pdf");

// the field value: exercises Č (U+010C), Ý (U+00DD), Ň (U+0147) - none are in
// WinAnsi but all are in CP-1250
const VALUE = "POČÍTAČ MODRÝ KAMEŇ";

// PDF text string as UTF-16BE with a BOM, written as a hex string <...>
function utf16beHex(s: string): string {
  let hex = "feff";
  for (const ch of s) {
    const cp = ch.codePointAt(0)!;
    hex += cp.toString(16).padStart(4, "0");
  }
  return `<${hex}>`;
}

function buildPdf(): Buffer {
  const v = utf16beHex(VALUE);
  const content = "q Q";

  const objs: string[] = [];
  // 1 Catalog
  objs.push("<< /Type /Catalog /Pages 2 0 R /AcroForm 6 0 R >>");
  // 2 Pages
  objs.push("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  // 3 Page (the widget is its only annotation)
  objs.push(
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 360 120] " +
      "/Resources << >> /Annots [5 0 R] /Contents 7 0 R >>",
  );
  // 4 base-14 font used by the field's default appearance
  objs.push("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  // 5 the text field widget: has a value but no /AP (must be synthesised)
  objs.push(
    "<< /Type /Annot /Subtype /Widget /FT /Tx /T (f1) " +
      `/V ${v} /DA (/Helv 0 Tf 0 g) /Rect [10 40 350 85] /P 3 0 R /F 4 >>`,
  );
  // 6 AcroForm: NeedAppearances forces synthesis; DR provides the DA font
  objs.push(
    "<< /Fields [5 0 R] /NeedAppearances true /DA (/Helv 0 Tf 0 g) " +
      "/DR << /Font << /Helv 4 0 R >> >> >>",
  );
  // 7 page content (empty)
  objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);

  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
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

writeFileSync(OUT, buildPdf());
console.log(`wrote ${OUT}`);
