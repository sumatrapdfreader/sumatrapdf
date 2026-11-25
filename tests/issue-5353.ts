// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5353
//
// Verifies -print-settings "portrait" / "landscape" set printer paper orientation
// and "docname=" sets the document name (#5065).
//
// Run:  bun tests/issue-5353.ts [--no-build]

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, ROOT } from "./util.ts";
import { pdfMediaBox, requirePrintToPdf, runPrintToPdf, tempPrintOutput } from "./print-util.ts";

const INPUT = join(ROOT, "tests", "issue-5597.pdf");
const DOCNAME = "SumatraTestDoc-5065";

export async function testit(): Promise<void> {
  requirePrintToPdf();

  const portraitOut = tempPrintOutput("5353-portrait");
  const portraitRes = runPrintToPdf(INPUT, portraitOut, `portrait,disable-auto-rotation,noscale,docname=${DOCNAME}`);
  if (!portraitRes.ok) {
    throw new Error(`portrait print failed (exit ${portraitRes.exitCode})`);
  }
  const portraitBox = pdfMediaBox(portraitOut);
  if (portraitBox.h <= portraitBox.w) {
    throw new Error(`portrait print expected tall MediaBox, got ${portraitBox.w}x${portraitBox.h}`);
  }
  const portraitData = readFileSync(portraitOut);
  const docNameUtf16 = Buffer.from(DOCNAME, "utf16le").swap16();
  if (!portraitData.includes(DOCNAME) && !portraitData.includes(docNameUtf16)) {
    throw new Error(`portrait print did not record document name "${DOCNAME}"`);
  }

  const landscapeOut = tempPrintOutput("5353-landscape");
  const landscapeRes = runPrintToPdf(INPUT, landscapeOut, "landscape,disable-auto-rotation,noscale");
  if (!landscapeRes.ok) {
    throw new Error(`landscape print failed (exit ${landscapeRes.exitCode})`);
  }
  const landscapeBox = pdfMediaBox(landscapeOut);
  if (landscapeBox.w <= landscapeBox.h) {
    throw new Error(`landscape print expected wide MediaBox, got ${landscapeBox.w}x${landscapeBox.h}`);
  }

  console.log("PASS issue-5353: portrait/landscape print-settings set paper orientation");
}

if (import.meta.main) {
  await runStandalone(testit);
}
