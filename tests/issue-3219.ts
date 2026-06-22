// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3219
//
// Searching for "emergency" failed because mupdf text extraction dropped the
// word. The table in this PDF uses embedded subset fonts (MSTT*) that name
// glyphs "G45", "G6D", ... (i.e. "G" + the hex character code) and ship no
// ToUnicode CMap, so mupdf couldn't map them to Unicode and emitted U+FFFD.
//
// This test extracts the page text with mupdf (the -extract-text harness) and,
// when pdfjs-dist is installed, also with pdf.js, and asserts both contain
// "Emergency". pdf.js recovers these glyph names in _simpleFontToUnicode(); the
// mupdf fix mirrors that heuristic in pdf_load_to_unicode (pdf-unicode.c).
//
// Run:  bun tests/issue-3219.ts [--no-build]   (or via tests/all.ts)

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-3219.pdf");

// extract page text via mupdf using the -extract-text harness. It prints the
// text as hex bytes (to avoid console locale mangling); we decode it back.
// The GUI exe's stdout doesn't reach a Bun pipe directly, but PowerShell (a
// console app, which Bun can pipe) relays it.
function extractWithMupdf(pdf: string): string {
  const psCmd = `& '${EXE}' -for-testing -extract-text -1 '${pdf}' 2>&1 | Out-String -Width 100000`;
  const p = Bun.spawnSync(["powershell", "-NoProfile", "-Command", psCmd]);
  const raw = p.stdout.toString();
  let all = "";
  for (const m of raw.matchAll(/text on page \d+: '([0-9a-f ]*)'/g)) {
    const hex = m[1].trim();
    if (!hex) {
      continue;
    }
    const bytes = hex.split(/\s+/).map((h) => parseInt(h, 16));
    all += Buffer.from(bytes).toString("utf8");
  }
  // '_' is the harness's stand-in for newline
  return all.split("_").join("\n");
}

// extract page text via pdf.js; returns null if pdfjs-dist isn't installed
async function extractWithPdfjs(pdf: string): Promise<string | null> {
  let getDocument: any;
  try {
    ({ getDocument } = await import("pdfjs-dist/legacy/build/pdf.mjs"));
  } catch {
    return null;
  }
  const data = new Uint8Array(readFileSync(pdf));
  const doc = await getDocument({ data }).promise;
  let all = "";
  for (let p = 1; p <= doc.numPages; p++) {
    const page = await doc.getPage(p);
    const tc = await page.getTextContent();
    for (const it of tc.items as any[]) {
      if (typeof it.str === "string") {
        all += it.str + (it.hasEOL ? "\n" : " ");
      }
    }
  }
  return all;
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-3219: fixture not found: ${PDF}`);
    return;
  }

  const mupdfText = extractWithMupdf(PDF);
  const mupdfHas = mupdfText.includes("Emergency");
  console.log(`mupdf: ${mupdfText.length} chars, has "Emergency": ${mupdfHas}`);

  // pdf.js comparison (the better baseline); optional if pdfjs-dist is absent
  const pdfjsText = await extractWithPdfjs(PDF);
  if (pdfjsText === null) {
    console.log("pdf.js: pdfjs-dist not installed (run `bun install` in tests/) - skipping comparison");
  } else {
    const pdfjsHas = pdfjsText.includes("Emergency");
    console.log(`pdf.js: ${pdfjsText.length} chars, has "Emergency": ${pdfjsHas}`);
    if (!pdfjsHas) {
      throw new Error("pdf.js did not extract 'Emergency' - unexpected, the fixture/baseline changed");
    }
  }

  if (!mupdfHas) {
    throw new Error("mupdf text extraction is missing 'Emergency' (issue #3219 regressed)");
  }
  console.log("PASS: mupdf extracts 'Emergency' (issue #3219)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
