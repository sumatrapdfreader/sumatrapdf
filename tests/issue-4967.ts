// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4967
//
// Verifies -print-settings "last" and "-1" print only the final page via
// Microsoft Print to PDF.
//
// Run:  bun tests/issue-4967.ts [--no-build]

import { join } from "node:path";
import { runStandalone } from "./util.ts";
import {
  pdfPageCount,
  requirePrintToPdf,
  runPrintToPdf,
  tempPrintOutput,
  writeMultiPagePdf,
} from "./print-util.ts";

const FIXTURE = join(import.meta.dir, "issue-4967.pdf");
const PAGES = ["Page 1", "Page 2", "Page 3"];

export async function testit(): Promise<void> {
  requirePrintToPdf();
  writeMultiPagePdf(FIXTURE, PAGES);
  if (pdfPageCount(FIXTURE) !== PAGES.length) {
    throw new Error(`fixture should have ${PAGES.length} pages`);
  }

  for (const c of [{ name: "last", settings: "last" }, { name: "-1", settings: "-1" }]) {
    const out = tempPrintOutput(`4967-${c.name}`);
    const res = runPrintToPdf(FIXTURE, out, c.settings);
    if (!res.ok) {
      throw new Error(`${c.name}: print failed (exit ${res.exitCode})`);
    }
    if (pdfPageCount(out) !== 1) {
      throw new Error(`${c.name}: expected 1 printed page, got ${pdfPageCount(out)}`);
    }
  }

  const allOut = tempPrintOutput("4967-all");
  const allRes = runPrintToPdf(FIXTURE, allOut, "");
  if (!allRes.ok || pdfPageCount(allOut) !== PAGES.length) {
    throw new Error("control print of all pages failed");
  }

  console.log("PASS issue-4967: last-page print settings work");
}

if (import.meta.main) {
  await runStandalone(testit);
}