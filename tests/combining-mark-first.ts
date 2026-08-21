// Regression test for a MuPDF text extraction crash when the first extracted
// character is a Unicode non-spacing mark (UCDN_GENERAL_CATEGORY_MN).
//
// The fixture maps a shown glyph to U+0301 COMBINING ACUTE ACCENT through a
// ToUnicode CMap. To regenerate it, use the PDF generator from the crash-fix
// notes or create an equivalent one-page PDF whose first extracted rune is
// U+0301.
//
// Run:  bun tests/combining-mark-first.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "combining-mark-first.pdf");

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    throw new Error(`fixture not found: ${PDF}`);
  }

  const text = extractPageText(PDF, 1);
  const hex = Buffer.from(text, "utf8").toString("hex").replace(/(..)/g, "$1 ").trim();
  console.log(`text on page 1: '${hex}'`);
  // U+0301 COMBINING ACUTE ACCENT as UTF-8
  if (!hex.startsWith("cc 81")) {
    throw new Error(`expected extracted text to start with U+0301 bytes "cc 81", got: '${hex}'`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
