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
import { EXE, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "combining-mark-first.pdf");

function extractPageTextHex(pdf: string): string {
  const psCmd = `& '${EXE}' -for-testing -extract-text -1 '${pdf}' 2>&1 | Out-String -Width 100000`;
  const p = Bun.spawnSync(["powershell", "-NoProfile", "-Command", psCmd]);
  const out = p.stdout.toString();
  if (p.exitCode !== 0) {
    throw new Error(`-extract-text failed with exit code ${p.exitCode}: ${out.trim()}`);
  }

  const m = out.match(/text on page 1: '([0-9a-f ]*)'/);
  if (!m) {
    throw new Error(`missing text extraction output: ${out.trim()}`);
  }
  return m[1].trim();
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    throw new Error(`fixture not found: ${PDF}`);
  }

  const hex = extractPageTextHex(PDF);
  console.log(`text on page 1: '${hex}'`);
  if (!hex.startsWith("cc 81")) {
    throw new Error(`expected extracted text to start with U+0301 bytes "cc 81", got: '${hex}'`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
