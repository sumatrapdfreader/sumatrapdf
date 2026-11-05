// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6485
//
// Copying Arabic text jumbled its diacritics. LibreOffice writes each letter
// with its marks as an ActualText, in logical order, while the glyphs around it
// are in visual order. The mark glyph's unicode is bogus, so it split the RTL
// run, and mupdf's run reversal also reversed the cluster. A glyph whose
// unicode is several chars (the "لا" ligature) was reversed the same way.
// Fixed in mupdf: ext/patches/0084-stext-rtl-clusters.patch.
//
// Fixture: tests/issue-6485.pdf (the PDF attached to #6483).
//
// Run:  bun tests/issue-6485.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-6485.pdf");

// letters with diacritics (ActualText) and ligatures (multi-char unicode)
const WANTED = [
  "ألا ليت",
  "الضلالة بالهدى",
  "أرض الأعاديِّ بعدما",
  "نائيًّا",
  "أَوْدَ وصُحْبَتِي",
  "الطَبَسَيْن فالتفتُّ",
  "الهوى لمَّا دعاني",
  "أن أُلام ردائيا",
];

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-6485: fixture not found: ${PDF}`);
    return;
  }

  // NFD puts a letter's marks in canonical order, whichever order the PDF gives
  const text = extractPageText(PDF).normalize("NFD");
  for (const w of WANTED) {
    if (!text.includes(w.normalize("NFD"))) {
      throw new Error(`extracted text is missing '${w}' (issue #6485)`);
    }
  }
  console.log("PASS: Arabic text keeps diacritics and ligatures in order (issue #6485)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
