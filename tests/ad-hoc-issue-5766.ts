// Ad-hoc regression test for issue #5766: copy/paste dropped a letter from
// doubled letters ("ll", "ff") in small fonts because the MuPDF duplicate-glyph
// filter (HasSeenGlyph) treated two *adjacent* identical letters as a redraw of
// one glyph. Only some lines were hit (sub-pixel rounding), e.g. lines 2 & 4 of
// the test doc became "Yelow Controler and Cofee".
//
// Exercises the real copy/selection path: TestTripleClickLineSelect finds a word
// via GetTextForPage() (-> FzTextPageToWStr -> HasSeenGlyph) and selects+extracts
// its line through GetSelectedTextTemp(). We assert that:
//   - a well-formed line ("...Coffee") is selectable and intact, and
//   - the corrupted forms the bug produced are absent from the page text
//     (FindWordCenter does substring matching, so if "Yelow" existed it'd be
//     found; with the fix it doesn't).
//
//   bun tests/ad-hoc-issue-5766.ts [--no-build]

import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

const PDF = "C:\\Users\\kjk\\Downloads\\bug-5766-bad-text-copy.pdf";
const GOOD_LINE = "Yellow Controller and Coffee";
// forms the bug produced by dropping a doubled letter
const CORRUPTED = ["Yelow", "Controler", "Cofee"];

async function selectLine(clickWord: string, expected: string): Promise<[number, string]> {
  const [exit, raw] = await runControlCommand(EXE, ControlCommand.TestTripleClickLineSelect, [
    PDF,
    clickWord,
    expected,
  ]);
  return [Number(exit), String(raw ?? "").trim()];
}

export async function testit(): Promise<void> {
  // a well-formed line must be selectable with all doubled letters intact
  const [, res] = await selectLine("Coffee", GOOD_LINE);
  console.log(`  select "Coffee" line -> ${res}`);
  if (!res.startsWith("OK ") || !res.includes(GOOD_LINE)) {
    throw new Error(`expected intact line "${GOOD_LINE}", got: ${res}`);
  }

  // the corrupted forms must not exist anywhere in the extracted page text.
  // On the buggy build FindWordCenter finds them (they're in lines 2 & 4) and
  // selects that line; with the fix they're absent -> "word-not-found".
  for (const bad of CORRUPTED) {
    const [, r] = await selectLine(bad, "__na__");
    console.log(`  probe "${bad}" -> ${r}`);
    if (!r.includes("word-not-found")) {
      throw new Error(`corrupted word "${bad}" is present in copied text (dropped-letter bug): ${r}`);
    }
  }
  console.log("  ✅ doubled letters preserved on copy (issue #5766)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
