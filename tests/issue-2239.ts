// Regression test for issue #2239: auto-linkify must not glue the next row of
// a two-column table onto a URL (https://www.archive.org/ + "Languagehat"),
// and must still join a URL that wraps on the next line at the same indent.
// Auto-links that overlap a real PDF /Link annot are dropped.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(content: string, annots: string): Buffer {
  const font = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const stream = `BT /F1 12 Tf\n${content}ET`;
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R${annots} >>`,
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    font,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

// 12pt type, 20pt leading so FzTextPageToUtf8 emits a hard newline (soft
// joins would turn the wrap into a space and stop the URL at "/").
const twoCol = `1 0 0 1 70 700 Tm (The Internet Archive ) Tj
1 0 0 1 280 700 Tm (https://www.archive.org/) Tj
1 0 0 1 70 680 Tm (Languagehat ) Tj
1 0 0 1 280 680 Tm (https://languagehat.com/) Tj
`;

const wrap = `1 0 0 1 72 500 Tm (https://example.com/foo/) Tj
1 0 0 1 72 480 Tm (bar) Tj
`;

function valuesFromDump(out: string): string[] {
  return [...out.matchAll(/^kind=\S+ page=-?\d+.* value=(.*)$/gm)].map((m) => m[1]!.trim());
}

async function pageLinks(client: ControlClient, pdf: string): Promise<string[]> {
  const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
  const out = String(res[1] ?? "");
  if (res[0] !== 0 && !out.includes("value=")) {
    throw new Error(`issue-2239: TestPageLinks failed for ${pdf}:\n${out}`);
  }
  return valuesFromDump(out);
}

export async function testit(): Promise<void> {
  const twoColPdf = tmpPath("issue-2239-twocol.pdf");
  const wrapPdf = tmpPath("issue-2239-wrap.pdf");
  const annotPdf = tmpPath("issue-2239-annot.pdf");
  writeFileSync(twoColPdf, makePdf(twoCol, ""));
  writeFileSync(wrapPdf, makePdf(wrap, ""));
  // Real annots on both columns; generous boxes so they overlap the text.
  const annots =
    " /Annots [" +
    "<< /Type /Annot /Subtype /Link /Rect [70 698 204 716] /A << /S /URI /URI (https://www.archive.org/) >> >> " +
    "<< /Type /Annot /Subtype /Link /Rect [280 698 500 716] /A << /S /URI /URI (https://www.archive.org/) >> >> " +
    "<< /Type /Annot /Subtype /Link /Rect [70 678 204 696] /A << /S /URI /URI (https://languagehat.com/) >> >> " +
    "<< /Type /Annot /Subtype /Link /Rect [280 678 500 696] /A << /S /URI /URI (https://languagehat.com/) >> >> " +
    "]";
  writeFileSync(annotPdf, makePdf(twoCol, annots));

  await withControlledSumatra(EXE, async (client) => {
    const twoColVals = await pageLinks(client, twoColPdf);
    const glued = twoColVals.filter((v) => v.includes("Languagehat") && v.includes("archive.org"));
    if (glued.length > 0) {
      throw new Error(`issue-2239: two-column auto-link glued the next row: ${glued.join(", ")}`);
    }
    if (!twoColVals.includes("https://www.archive.org/")) {
      throw new Error(`issue-2239: missing archive.org auto-link, got:\n${twoColVals.join("\n")}`);
    }
    if (!twoColVals.includes("https://languagehat.com/")) {
      throw new Error(`issue-2239: missing languagehat.com auto-link, got:\n${twoColVals.join("\n")}`);
    }

    const wrapVals = await pageLinks(client, wrapPdf);
    if (!wrapVals.includes("https://example.com/foo/bar")) {
      throw new Error(`issue-2239: wrapped URL was not joined, got:\n${wrapVals.join("\n")}`);
    }

    const annotVals = await pageLinks(client, annotPdf);
    const gluedAnnot = annotVals.filter((v) => v.includes("Languagehat") && v.includes("archive.org"));
    if (gluedAnnot.length > 0) {
      throw new Error(`issue-2239: auto-link overlapped a real annot: ${gluedAnnot.join(", ")}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
