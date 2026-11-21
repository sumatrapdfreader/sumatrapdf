// Copying "פח עובי 5" from a SOLIDWORKS drawing gave "פח עובי5". The PDF
// writes the space first but draws it between the Hebrew word and the
// number, so it led the line and was dropped as leading whitespace.
//
// Fixture: tests/rtl-leading-space.pdf (tests/rtl-leading-space-make-fixture.py).
//
// Run:  bun tests/rtl-leading-space.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "rtl-leading-space.pdf");
const WANTED = "פח עובי 5";

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    throw new Error(`fixture not found: ${PDF}`);
  }

  const text = extractPageText(PDF, 1).trim();
  console.log(`extracted: '${text}'`);
  if (text !== WANTED) {
    throw new Error(`want '${WANTED}', got '${text}'`);
  }
  console.log("PASS: leading space of right-to-left run lands where it is drawn");
}

if (import.meta.main) {
  await runStandalone(testit);
}
