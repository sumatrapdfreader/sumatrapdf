// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5597
//
// Case-insensitive search should find Turkish/accented text regardless of case.
// The reported example: searching "ibradı" (lowercase i) should find the word
// "İbradı" (capital dotted İ, U+0130).
//
// This drives the headless `-test-search` flag against tests/issue-5597.pdf
// (which contains "İbradı", "CAFÉ" and "Ankara") and checks each search.
//
// Background: case folding goes through CharLowerW, which is LOCALE-DEPENDENT
// for U+0130 (İ) -- it only folds İ->i under a Turkish system locale, so on
// every other locale "ibradı" would not match "İbradı". FoldCaseForSearch now
// special-cases U+0130 -> 'i' (standard Unicode case folding) so the search is
// case-insensitive regardless of locale. This test fails if that fold is
// removed (verified).
//
// Run:  bun tests/issue-5597.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";

const SCRIPT_DIR = import.meta.dir;
const ROOT = join(SCRIPT_DIR, "..");
const EXE = join(ROOT, "out", "dbg64", "SumatraPDF-dll.exe");
const PDF = join(SCRIPT_DIR, "issue-5597.pdf");

function fail(msg: string): never {
  console.error(`\n❌ ${msg}`);
  process.exit(1);
}

function buildApp() {
  console.log("• building SumatraPDF-dll.exe (cmd/build.ts) ...");
  const p = Bun.spawnSync({ cmd: ["bun", join(ROOT, "cmd", "build.ts")], cwd: ROOT, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    fail("build failed");
  }
}

// returns the 1-based page the needle was found on, or 0 if not found
function search(needle: string): number {
  const out = join(SCRIPT_DIR, "issue-5597-search-result.txt");
  try {
    require("node:fs").rmSync(out, { force: true });
  } catch {}
  const p = Bun.spawnSync({ cmd: [EXE, "-test-search", PDF, needle, out], stdout: "pipe", stderr: "pipe" });
  const raw = existsSync(out) ? require("node:fs").readFileSync(out, "utf-8").trim() : p.stdout.toString().trim();
  try {
    require("node:fs").rmSync(out, { force: true });
  } catch {}
  const m = raw.match(/FOUND .*page=(\d+)/);
  return m ? parseInt(m[1]) : 0;
}

function main() {
  if (!process.argv.includes("--no-build")) {
    buildApp();
  }
  if (!existsSync(EXE)) {
    fail(`app not found: ${EXE} (run without --no-build)`);
  }
  if (!existsSync(PDF)) {
    fail(`test pdf not found: ${PDF}`);
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
    const page = search(s.needle);
    const ok = page === 1;
    sanityOk &&= ok;
    console.log(`    ${ok ? "✅" : "❌"} "${s.needle}" (${s.desc}) -> ${page ? `page ${page}` : "NOT FOUND"}`);
  }
  if (!sanityOk) {
    fail("sanity checks failed — harness/pdf problem, not the issue under test");
  }

  // the headline #5597 case: lowercase 'i' must match capital dotted 'İ' (U+0130)
  console.log('\n• issue #5597: searching "ibradı" should find "İbradı":');
  const page = search("ibradı");
  if (page === 1) {
    console.log('    ✅ FOUND on page 1 — issue #5597 is fixed on this system');
    process.exit(0);
  }
  console.log("    ❌ NOT FOUND — issue #5597 is NOT fixed on this (non-Turkish) locale.");
  console.log("       CharLowerW(U+0130 İ) is locale-dependent and returns İ unchanged here,");
  console.log("       so İ never folds to 'i'. A locale-independent fold is needed.");
  process.exit(1);
}

main();
