// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5597
//
// Case-insensitive search should find Turkish/accented text regardless of case.
// The reported example: searching "ibradı" (lowercase i) should find the word
// "İbradı" (capital dotted İ, U+0130).
//
// This drives the control pipe search command against tests/issue-5597.pdf
// (which contains "İbradı", "CAFÉ" and "Ankara") and checks each search.
//
// Background: case folding goes through CharLowerW, which is LOCALE-DEPENDENT
// for U+0130 (İ) -- it only folds İ->i under a Turkish system locale, so on
// every other locale "ibradı" would not match "İbradı". FoldCaseForSearch now
// special-cases U+0130 -> 'i' (standard Unicode case folding) so the search is
// case-insensitive regardless of locale. This test fails if that fold is
// removed (verified).
//
// Run:  bun tests/issue-5597.ts [--no-build]   (or via tests/all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";

const PDF = join(import.meta.dir, "issue-5597.pdf");

// returns the 1-based page the needle was found on, or 0 if not found
async function search(needle: string): Promise<number> {
  const [, rawArg] = await runControlCommand(EXE, ControlCommand.TestSearch, [PDF, needle]);
  const raw = String(rawArg).trim();
  const m = raw.match(/FOUND .*page=(\d+)/);
  return m ? parseInt(m[1]) : 0;
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  if (!existsSync(PDF)) {
    throw new Error(`test pdf not found: ${PDF}`);
  }

  // locale-independent sanity checks — these must pass (verify the harness +
  // pdf + accented folding all work)
  const sanity = [
    { needle: "Ankara", desc: "exact word" },
    { needle: "ankara", desc: "ascii case-insensitive" },
    { needle: "CAFÉ", desc: "exact accented word" },
    { needle: "café", desc: "accented case-insensitive (É->é)" },
    { needle: "İbradı", desc: "exact Turkish word" },
  ];
  console.log("• sanity checks (must all find on page 1):");
  let sanityOk = true;
  for (const s of sanity) {
    const page = await search(s.needle);
    const ok = page === 1;
    sanityOk &&= ok;
    console.log(`    ${ok ? "✅" : "❌"} "${s.needle}" (${s.desc}) -> ${page ? `page ${page}` : "NOT FOUND"}`);
  }
  if (!sanityOk) {
    throw new Error("sanity checks failed — harness/pdf problem, not the issue under test");
  }

  // the headline #5597 case: lowercase 'i' must match capital dotted 'İ' (U+0130)
  console.log('\n• issue #5597: searching "ibradı" should find "İbradı":');
  const page = await search("ibradı");
  if (page === 1) {
    console.log('    ✅ FOUND on page 1 — issue #5597 is fixed on this system');
    return;
  }
  console.log("    ❌ NOT FOUND — issue #5597 is NOT fixed on this (non-Turkish) locale.");
  console.log("       CharLowerW(U+0130 İ) is locale-dependent and returns İ unchanged here,");
  console.log("       so İ never folds to 'i'. A locale-independent fold is needed.");
  throw new Error('searching "ibradı" did not find "İbradı"');
}

if (import.meta.main) {
  await runStandalone(testit);
}
