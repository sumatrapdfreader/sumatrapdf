// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5065
//
// Verifies -print-settings "docname=..." sets the Windows print document name.
//
// Run:  bun tests/issue-5065.ts [--no-build]

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, ROOT } from "./util.ts";
import { requirePrintToPdf, runPrintToPdf, tempPrintOutput } from "./print-util.ts";

const INPUT = join(ROOT, "tests", "issue-5597.pdf");
const DOCNAME = "SumatraTestDoc-5065";

function pdfContainsDocName(path: string, docName: string): boolean {
  const data = readFileSync(path);
  const utf16be = Buffer.from(docName, "utf16le").swap16();
  return data.includes(docName) || data.includes(utf16be);
}

export async function testit(): Promise<void> {
  requirePrintToPdf();
  const out = tempPrintOutput("5065");
  const res = runPrintToPdf(INPUT, out, `docname=${DOCNAME}`);
  if (!res.ok) {
    throw new Error(`print failed (exit ${res.exitCode})`);
  }
  if (!pdfContainsDocName(out, DOCNAME)) {
    throw new Error(`expected printed PDF metadata to contain "${DOCNAME}"`);
  }
  console.log("PASS issue-5065: docname= sets print document title");
}

if (import.meta.main) {
  await runStandalone(testit);
}