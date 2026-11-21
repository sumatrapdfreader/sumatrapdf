// Ad-hoc triple-click line selection test (issue #5712).
//
// Builds a single-line PDF ("To our shareholders"), simulates a triple-click in
// the middle of "shareholders" via -dbg-control TestTripleClickLineSelect, and
// checks the selected text is the full line.
//
// NOT registered in tests/run-almost-all.ts — run directly:
//   bun tests/ad-hoc-triple-click-line.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, runControlCommand } from "./control.ts";
import { EXE, copyFromOneDrive, runStandalone, tmpPath, makeOneLinePdf } from "./util.ts";

const LINE = "To our shareholders";
const CLICK_WORD = "shareholders";

const AMAZON_LINE = "To our shareholders:";

async function runTripleClickCase(pdfPath: string, expectedLine: string, label: string): Promise<void> {
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
  writeFileSync(pdfPath, makeOneLinePdf(LINE));
  await runTripleClickCase(pdfPath, LINE, `triple-click line select on "${LINE}"`);

  const amazonPdf = tmpPath("All Amazon Shareholder Letters.pdf");
  if (copyFromOneDrive("!sumatra/All Amazon Shareholder Letters.pdf", amazonPdf)) {
    await runTripleClickCase(
      amazonPdf,
      AMAZON_LINE,
      `triple-click line select on Amazon letters page 1 ("${AMAZON_LINE}")`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
