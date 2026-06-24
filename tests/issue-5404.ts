// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5404
//
// A PDF form field whose value contains Central European Latin diacritics
// (Č, Ň, Ý, ...) rendered blank/garbled. With /AcroForm /NeedAppearances the
// viewer synthesises the field appearance from the field value; mupdf routed
// any text with characters outside WinAnsi/Greek/Cyrillic/CJK through the
// rich/HTML layout path, which dropped those glyphs. The fix adds a CP-1250
// (Latin-2) path to the simple appearance synthesis (mupdf pdf-appearance.c /
// pdf-font-add.c), so "POČÍTAČ MODRÝ KAMEŇ" renders normally.
//
// The fixture is a minimal one-page PDF with a single such field and no baked
// appearance, forcing synthesis. Regenerate with: bun tests/issue-5404-make.ts
//
// This extracts the synthesised field text with mupdf (the -extract-text
// harness) and asserts it contains the field value; before the fix the
// extraction is garbled ("POČÍTAMDRÝKEŇ"). When pdfjs-dist is installed it also
// confirms pdf.js reads the same field value (the baseline).
//
// Run:  bun tests/issue-5404.ts [--no-build]   (or via tests/all.ts)

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-5404.pdf");
const TARGETS = ["POČÍTAČ", "MODRÝ", "KAMEŇ"];

// extract text via mupdf using the -extract-text harness. It prints the text as
// hex bytes (to avoid console locale mangling); we decode it back. The GUI exe's
// stdout doesn't reach a Bun pipe directly, but PowerShell (a console app, which
// Bun can pipe) relays it.
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

// read the field value via pdf.js getAnnotations(); returns null if pdfjs-dist
// isn't installed. (pdf.js getTextContent does not cover widget appearances, so
// we read the field value directly - the baseline the synthesis must match.)
async function fieldValuesWithPdfjs(pdf: string): Promise<string | null> {
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
    for (const a of (await page.getAnnotations()) as any[]) {
      if (typeof a.fieldValue === "string") {
        all += a.fieldValue + " ";
      }
    }
  }
  return all;
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-5404: fixture not found: ${PDF}`);
    return;
  }

  const mupdfText = extractWithMupdf(PDF);
  console.log(`mupdf: ${JSON.stringify(mupdfText.trim())}`);

  // pdf.js baseline (optional if pdfjs-dist is absent)
  const pdfjsText = await fieldValuesWithPdfjs(PDF);
  if (pdfjsText === null) {
    console.log("pdf.js: pdfjs-dist not installed (run `bun install` in tests/) - skipping comparison");
  } else {
    for (const t of TARGETS) {
      if (!pdfjsText.includes(t)) {
        throw new Error(`pdf.js did not read '${t}' - unexpected, the fixture/baseline changed`);
      }
    }
  }

  for (const t of TARGETS) {
    if (!mupdfText.includes(t)) {
      throw new Error(`mupdf appearance synthesis dropped '${t}' (issue #5404 regressed): got ${JSON.stringify(mupdfText.trim())}`);
    }
  }
  console.log(`PASS: mupdf synthesises Central European diacritics in form fields (issue #5404)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
