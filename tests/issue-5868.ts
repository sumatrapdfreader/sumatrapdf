// #5868: copied text lost its word spacing. The tracking-space filter added for
// #5627 dropped any space whose glyph gap was under 0.2em, and any MuPDF
// synthetic space under 0.35em. A PDF that positions words with TJ offsets
// instead of space glyphs (groff/troff output, e.g. a man page) has *every*
// word space synthesized at a normal 0.25-0.3em gap, so nearly all of them
// were deleted.
//
// Also checks the #5627 direction still holds, if that repro file is around:
// the filter must still drop the near-zero "tracking" spaces that split words
// into syllables.
//
// Run:  bun tests/issue-5868.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, ROOT, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  // collapse soft newlines so multi-line phrases still match with spaces
  const text = extractPageText(pdf, 1).replaceAll("\n", " ");
  // spans a font change (roman -> bold -> roman), where the spaces used to go
  const wanted = ["Library Functions Manual", "see zlib.h for full description", "The zlib library is a general"];
  for (const w of wanted) {
    if (!text.includes(w)) {
      throw new Error(`extracted text is missing word spacing: expected '${w}'`);
    }
  }

  // the #5627 repro isn't in the repo (see agents.md); check it when present
  const repro = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-5627.pdf";
  if (!existsSync(repro)) {
    console.log(`issue-5868: SKIP tracking-space half (no ${repro})`);
    return;
  }
  const trk = extractPageText(repro, 1).replaceAll("\n", " ");
  for (const bad of ["kro nik", "im mün", "dün ya"]) {
    if (trk.includes(bad)) {
      throw new Error(`tracking spaces are back: extracted text contains '${bad}'`);
    }
  }
  // ...without swallowing the real word spaces around them
  for (const w of ["Helicobacter pylori", "Turkiye Klinikleri J Dermatol"]) {
    if (!trk.includes(w)) {
      throw new Error(`tracking-space filter ate a real word space: expected '${w}'`);
    }
  }
  console.log("PASS: word spacing preserved (issue #5868); tracking still filtered (#5627)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
