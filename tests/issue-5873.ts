// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5873
//
// Distiller-produced Type1 PDFs (Literaturnaya / Academy) often ship a
// ToUnicode CMap that identity-maps 0xC0-0xFF to Latin-1 U+00C0-U+00FF, derived
// from Latin Encoding glyph names, while the content stream uses Windows-1251
// codes for Cyrillic. mupdf then extracts mojibake ("ÏÐÅÄÈÑËÎÂÈÅ" instead of
// "ПРЕДИСЛОВИЕ"). The fix remaps via Windows-1251 when Encoding names are
// identity Latin-1 and the font name or document title/author looks Cyrillic.
//
// Fixture: tests/issue-5873.pdf (copy of the bug report PDF).
//
// Run:  bun tests/issue-5873.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-5873.pdf");
// ПРЕДИСЛОВИЕ
const TARGET_TITLE = "\u041F\u0420\u0415\u0414\u0418\u0421\u041B\u041E\u0412\u0418\u0415";
// Назначение
const TARGET_WORD = "\u041D\u0430\u0437\u043D\u0430\u0447\u0435\u043D\u0438\u0435";
// mojibake of the title if the bug is back (Latin-1 reading of CP1251 bytes)
const MOJIBAKE_TITLE = "\u00CF\u00D0\u00C5\u00C4\u00C8\u00D1\u00CB\u00CE\u00C2\u00C8\u00C5";

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-5873: fixture not found: ${PDF}`);
    return;
  }

  const text = extractPageText(PDF);
  console.log(`extracted ${text.length} chars`);

  if (text.includes(MOJIBAKE_TITLE)) {
    throw new Error(`mupdf still extracts Latin-1 mojibake for Russian title (issue #5873 regressed)`);
  }
  if (!text.includes(TARGET_TITLE)) {
    throw new Error(`mupdf text extraction is missing '${TARGET_TITLE}' (issue #5873)`);
  }
  if (!text.includes(TARGET_WORD)) {
    throw new Error(`mupdf text extraction is missing '${TARGET_WORD}' (issue #5873)`);
  }
  console.log(`PASS: mupdf extracts Russian text correctly (issue #5873)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
