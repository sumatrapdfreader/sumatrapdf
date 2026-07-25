// Tests for https://github.com/sumatrapdfreader/sumatrapdf/issues/5792
// and https://github.com/sumatrapdfreader/sumatrapdf/issues/5793
//
// #5792: mismatched FB2/XML closers (e.g. <b><i>x</b></i>) used to abort the
// MuPDF XML parse and produce giant fonts / huge page counts. Lenient close_tag
// recovery keeps normal layout — title text and body still extract.
//
// #5793: reflow FB2 copy used one hard newline per visual line. Soft-join in
// FzTextPageToUtf8 merges wraps within a paragraph with a space, while keeping
// newlines between paragraphs.
//
// Fixture: tests/issue-5792.fb2 (mismatched title tags + multi-line paragraphs).
// Run:  bun tests/issue-5792.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const FB2 = join(import.meta.dir, "issue-5792.fb2");

function extractPage(page: number, file: string): string {
  const psCmd = `& '${EXE}' -for-testing -extract-text ${page} '${file}' 2>&1 | Out-String -Width 100000`;
  const p = Bun.spawnSync(["powershell", "-NoProfile", "-Command", psCmd]);
  const raw = p.stdout.toString() + p.stderr.toString();
  let all = "";
  for (const m of raw.matchAll(/text on page \d+: '([0-9a-f ]*)'/g)) {
    const hex = m[1].trim();
    if (!hex) {
      continue;
    }
    const bytes = hex.split(/\s+/).map((h) => parseInt(h, 16));
    all += Buffer.from(bytes).toString("utf8");
  }
  return all.split("_").join("\n");
}

export async function testit(): Promise<void> {
  if (!existsSync(FB2)) {
    throw new Error(`fixture missing: ${FB2}`);
  }
  if (!existsSync(EXE)) {
    throw new Error(`exe missing: ${EXE}`);
  }

  const text = extractPage(1, FB2);
  console.log(`page 1 len=${text.length} newlines=${(text.match(/\n/g) || []).length}`);
  if (!text) {
    throw new Error("#5792: empty extract for fixture FB2");
  }

  // #5792: mismatched <b><i>…</b></i> must still yield the title (parse recovery)
  if (!text.includes("Chapter One")) {
    throw new Error(`#5792: missing title after mismatched tags:\n${JSON.stringify(text.slice(0, 400))}`);
  }
  if (!text.includes("Alpha paragraph") || !text.includes("Bravo paragraph")) {
    throw new Error(`#5792: body missing:\n${JSON.stringify(text.slice(0, 400))}`);
  }

  // #5793: soft-join wraps inside Alpha (no mid-paragraph hard breaks)
  const alphaIdx = text.indexOf("Alpha paragraph");
  const bravoIdx = text.indexOf("Bravo paragraph");
  const charlieIdx = text.indexOf("Charlie paragraph");
  if (bravoIdx <= alphaIdx || charlieIdx <= bravoIdx) {
    throw new Error("#5793: Alpha/Bravo/Charlie order wrong");
  }
  // Slice up to the next marker; trailing paragraph separator newlines are OK.
  const alphaBlock = text.slice(alphaIdx, bravoIdx).replace(/\n+$/, "");
  const bravoBlock = text.slice(bravoIdx, charlieIdx).replace(/\n+$/, "");
  if (alphaBlock.includes("\n")) {
    throw new Error(
      `#5793: Alpha still has hard line-breaks:\n${JSON.stringify(alphaBlock)}`,
    );
  }
  if (bravoBlock.includes("\n")) {
    throw new Error(
      `#5793: Bravo still has hard line-breaks:\n${JSON.stringify(bravoBlock)}`,
    );
  }
  // Paragraph boundaries kept as newlines (not soft-joined into one blob)
  if (!/\n\s*Bravo paragraph/.test(text) || !/\n\s*Charlie paragraph/.test(text)) {
    throw new Error(
      `#5793: expected hard newlines between paragraphs:\n${JSON.stringify(text)}`,
    );
  }

  // #5792 sanity: short sample is one page — page 2 must be empty / fail extract
  const page2 = extractPage(2, FB2);
  if (page2.includes("Alpha paragraph")) {
    throw new Error("#5792: body repeated on page 2 (unexpected giant-font pagination?)");
  }

  console.log("issue-5792/5793 ok");
}

if (import.meta.main) {
  await runStandalone(testit);
}
