// Ad-hoc triple-click line selection test (issue #5712).
//
// Builds a single-line PDF ("To our shareholders"), simulates a triple-click in
// the middle of "shareholders" via -dbg-control TestTripleClickLineSelect, and
// checks the selected text is the full line.
//
// NOT registered in tests/all.ts — run directly:
//   bun tests/ad-hoc-triple-click-line.ts [--no-build]
// or as part of: bun tests/before-release.ts

import { existsSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const LINE = "To our shareholders";
const CLICK_WORD = "shareholders";

const AMAZON_LETTERS_PDF = join(
  homedir(),
  "OneDrive",
  "!sumatra",
  "All Amazon Shareholder Letters.pdf",
);
const AMAZON_LINE = "To our shareholders:";

function buildPdf(line: string): Buffer {
  const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
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

async function runTripleClickCase(
  pdfPath: string,
  expectedLine: string,
  label: string,
): Promise<void> {
  console.log(`• ${label} ...`);
  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestTripleClickLineSelect, [
    pdfPath,
    CLICK_WORD,
    expectedLine,
  ]);
  const result = String(raw ?? "").trim();
  console.log(`  exit=${exitCode}`);
  console.log(`  result: ${result}`);

  if (Number(exitCode) !== 0 || !result.startsWith("OK ")) {
    throw new Error(`${label} failed: ${result}`);
  }
  const normalized = result.replace(/^OK selected=/, "").trim();
  if (normalized !== expectedLine && normalized !== `${expectedLine}\n`) {
    throw new Error(`${label} unexpected selection: ${normalized}`);
  }
  console.log(`  ✅ ${label} ok`);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5712.pdf");
  writeFileSync(pdfPath, buildPdf(LINE));
  await runTripleClickCase(pdfPath, LINE, `triple-click line select on "${LINE}"`);

  if (existsSync(AMAZON_LETTERS_PDF)) {
    await runTripleClickCase(
      AMAZON_LETTERS_PDF,
      AMAZON_LINE,
      `triple-click line select on Amazon letters page 1 ("${AMAZON_LINE}")`,
    );
  } else {
    console.log(`• skip Amazon letters PDF: not found at ${AMAZON_LETTERS_PDF}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}