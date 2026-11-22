// An auto-size (font size 0) single-line text field shrinks its text to fit,
// as it does for Latin text, when the text needs mupdf's rich layout (Thai,
// Hindi, Arabic). It kept the field's full height and wrapped, so the field
// showed the first few words and the rest were cut off.
//
// Run: bun tests/form-field-fit.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, loadPng, runStandalone, tmpPath } from "./util.ts";

const name = "form-field-fit";
const kWords = Array.from({ length: 10 }, (_, i) => `ไทย${i + 1}`);
// the field, in points on a 360 x 120 page
const kFieldX1 = 350;

function utf16beHex(s: string): string {
  let hex = "feff";
  for (const ch of s) {
    const cp = ch.codePointAt(0)!;
    if (cp >= 0x10000) {
      const c = cp - 0x10000;
      hex += (0xd800 + (c >> 10)).toString(16) + (0xdc00 + (c & 0x3ff)).toString(16);
    } else {
      hex += cp.toString(16).padStart(4, "0");
    }
  }
  return `<${hex}>`;
}

// one auto-size text field with `value` and no appearance, so it is made
function fieldPdf(value: string): string {
  const content = "q Q";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /AcroForm 6 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 360 120] /Resources << >> /Annots [5 0 R] /Contents 7 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    `<< /Type /Annot /Subtype /Widget /FT /Tx /T (f1) /V ${utf16beHex(value)} /DA (/Helv 0 Tf 0 g) ` +
      `/Rect [10 40 ${kFieldX1} 85] /P 3 0 R /F 4 >>`,
    "<< /Fields [5 0 R] /NeedAppearances true /DA (/Helv 0 Tf 0 g) /DR << /Font << /Helv 4 0 R >> >> >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  let pdf = "%PDF-1.7\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

function run(args: string[]): string {
  const p = Bun.spawnSync([EXE, "cli", ...args], { stdout: "pipe", stderr: "pipe" });
  if (p.exitCode !== 0) {
    throw new Error(`${name}: cli ${args[0]} failed: ${p.stderr.toString()}`);
  }
  return p.stdout.toString();
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "field.pdf");
  writeFileSync(pdf, fieldPdf(kWords.join(" ")), "latin1");

  const text = run(["text", pdf]);
  if (!text.includes(kWords[kWords.length - 1]!)) {
    throw new Error(`${name}: the field shows '${text.trim()}', not all ${kWords.length} words`);
  }
  // one line, inside the field
  const png = join(dir, "field.png");
  run(["render", pdf, "--dpi", "72", "-o", png]);
  const img = loadPng(png);
  let x1 = -1;
  // runs of rows with ink: one per line of text
  let bands = 0;
  let prevInk = false;
  for (let y = 0; y < img.h; y++) {
    let rowInk = false;
    for (let x = 0; x < img.w; x++) {
      if (img.data[(y * img.w + x) * img.nComp]! < 128) {
        x1 = Math.max(x1, x + 1);
        rowInk = true;
      }
    }
    if (rowInk && !prevInk) {
      bands++;
    }
    prevInk = rowInk;
  }
  if (bands !== 1) {
    throw new Error(`${name}: the text is on ${bands} lines, not one`);
  }
  if (x1 > kFieldX1) {
    throw new Error(`${name}: the text runs to x=${x1}, past the field's right edge ${kFieldX1}`);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
