// Test for issue #5724: generate a table of contents from numbered headings
// when the PDF has no outline.
//
// Run: bun tests/issue-5724.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, runControlCommand } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(opts: { outlines?: boolean }): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const page1 =
    "BT /F1 14 Tf 72 720 Td (I. Introduction) Tj ET\n" +
    "BT /F1 12 Tf 72 700 Td (Body text on page one without a heading number.) Tj ET";
  const page2 =
    "BT /F1 14 Tf 72 720 Td (II. Methods) Tj ET\n" +
    "BT /F1 14 Tf 72 700 Td (II.A. Nested section) Tj ET\n" +
    "BT /F1 12 Tf 72 680 Td (Body text on page two is not a heading.) Tj ET";
  const body: Record<number, Buffer> = {};
  if (opts.outlines) {
    body[1] = enc("<< /Type /Catalog /Pages 2 0 R /Outlines 8 0 R >>");
  } else {
    body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  }
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>");
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 7 0 R >> >> /Contents 4 0 R >>`,
  );
  body[4] = enc(`<< /Length ${page1.length} >>\nstream\n${page1}\nendstream`);
  body[5] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 7 0 R >> >> /Contents 6 0 R >>`,
  );
  body[6] = enc(`<< /Length ${page2.length} >>\nstream\n${page2}\nendstream`);
  body[7] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  let maxN = 7;
  if (opts.outlines) {
    body[8] = enc("<< /Type /Outlines /First 9 0 R /Last 9 0 R /Count 1 >>");
    body[9] = enc("<< /Title (From File) /Parent 8 0 R /Dest [3 0 R /XYZ 0 792 0] >>");
    maxN = 9;
  }
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

async function getToc(path: string): Promise<string> {
  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestGetToc, [path]);
  if (exitCode !== 0) {
    throw new Error(`issue-5724: TestGetToc failed: ${String(raw ?? "").trim()}`);
  }
  return String(raw ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5724");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const noOutline = join(dir, "no-outline.pdf");
  writeFileSync(noOutline, makePdf({ outlines: false }));
  const got = await getToc(noOutline);
  const expected = "I. Introduction|page=1\n" + "II. Methods|page=2\n" + "  II.A. Nested section|page=2\n";
  if (got !== expected) {
    throw new Error(`issue-5724: generated TOC mismatch.\nexpected:\n${expected}got:\n${got}`);
  }
  console.log("issue-5724: generated TOC from headings OK");

  const withOutline = join(dir, "with-outline.pdf");
  writeFileSync(withOutline, makePdf({ outlines: true }));
  const gotOutline = await getToc(withOutline);
  if (!gotOutline.includes("From File|page=1")) {
    throw new Error(`issue-5724: real outline should win, got:\n${gotOutline}`);
  }
  if (gotOutline.includes("I. Introduction")) {
    throw new Error(`issue-5724: should not generate headings when the file has an outline:\n${gotOutline}`);
  }
  console.log("issue-5724: existing outline still used");

  console.log("PASS: generate table of contents if missing (issue #5724)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
