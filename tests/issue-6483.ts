// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6483
//
// Copying Arabic text lost every space between words. The #5627 tracking-space
// filter measures from a space to the next letter and drops the space when the
// next letter starts less than 0.1em to its right. In right-to-left text the
// next letter is to the left, so that distance is negative and every word space
// looked like tracking.
//
// Fixture: tests/issue-6483.pdf (the issue's attachment, exported by LibreOffice).
//
// Run:  bun tests/issue-6483.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-6483.pdf");

// words from the first and second lines, in logical order
// "الغضا مل": the next letter sits at the space's own origin (gap 0)
const WANTED = ["ليت شعري هل", "بجنب الغضا", "وليت الغضا", "الغضا مل يقطع"];

// the page has ~70 words; with the bug it extracted with no spaces at all
const kMinSpaces = 50;

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-6483: fixture not found: ${PDF}`);
    return;
  }

  const text = extractPageText(PDF);
  const nSpaces = (text.match(/ /g) ?? []).length;
  console.log(`extracted ${text.length} chars, ${nSpaces} spaces`);

  if (nSpaces < kMinSpaces) {
    throw new Error(`Arabic word spaces were dropped: ${nSpaces} spaces, want at least ${kMinSpaces} (issue #6483)`);
  }
  for (const w of WANTED) {
    if (!text.includes(w)) {
      throw new Error(`extracted text is missing '${w}' (issue #6483)`);
    }
  }
  console.log("PASS: Arabic text keeps its word spaces (issue #6483)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
