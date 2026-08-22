// The suite the daily GitHub Actions job runs (.github/workflows/windows-daily.yml).
//
// Two things make it different from run-all.ts:
//
//  - it does not stop at the first failure. A daily run is only useful if it
//    tells us everything that broke, so every test runs and the failures are
//    listed at the end (and the process exits non-zero, so the run is marked
//    failed and we get notified).
//  - the app window covers the whole work area instead of half of it. A
//    hosted runner's screen is small and nobody is looking at it, so there is
//    nothing to stay out of the way of, and tests get room for toolbars,
//    sidebars and dialogs.
//
// It does not build: the workflow builds (debug ASan) and points
// SUMATRA_TEST_EXE at the resulting exe.
//
// Run:  bun tests/run-github-ci.ts [test-name ...]
//
// Naming tests (on the command line, or through the workflow's "tests" input)
// runs only those, which is how a single CI failure is iterated on without
// waiting for the whole suite.

import { existsSync } from "node:fs";
import { EXE, formatDuration, resetTestTimes, runTest, type NamedTest } from "./util.ts";
import { getWorkArea, setTestWindowLayout, testWindowPos } from "./winapi.ts";
import { tests as allTests } from "./run-all.ts";

// Tests that need something a hosted runner doesn't have. Keep the reason with
// the name: it is what tells the next person whether it is still true.
export const excludedTests: Record<string, string> = {
  // "Microsoft Print to PDF" is a Windows client feature; the runner images are
  // Windows Server and don't have it (nor any other printer)
  "issue-4967": "prints through Microsoft Print to PDF",
  "issue-5065": "prints through Microsoft Print to PDF",
  "issue-5353": "prints through Microsoft Print to PDF",
  // a TeX distribution is a big install we don't want on the runner; the LaTeX
  // suite itself (tests/latex.ts) is only in run-pre-release, not here
  "issue-5040": "needs pdflatex (MiKTeX / TeX Live)",
  // agents.md flags this one as focus-dependent even on a developer machine;
  // on a runner nothing has focus. Re-enable if it turns out to be fine.
  // (issue-1136 was the other one; it now waits on the home page's own state
  // over -dbg-control instead of on focus, so it runs here.)
  "issue-2254": "depends on keyboard focus, flaky without an interactive desktop",
  // the daily job builds SumatraPDF-static.exe, which does not embed IDR_DLL_PAK
  "issue-6025": "installer UI needs IDR_DLL_PAK, which the static CI exe does not embed",
};

export function ciTests(): NamedTest[] {
  return allTests.filter(([name]) => !excludedTests[name]);
}

type Failure = { name: string; msg: string };

// tests defaults to every test that can run here; pass a list to run a subset
// (a smoke check of this runner, or reproducing one CI failure locally)
export async function testit(tests: NamedTest[] = ciTests()): Promise<void> {
  setTestWindowLayout("workArea");
  resetTestTimes();

  const wa = getWorkArea();
  const pos = testWindowPos();
  console.log(`exe:       ${EXE}`);
  console.log(`work area: ${wa.right - wa.left}x${wa.bottom - wa.top}`);
  console.log(`window:    ${pos.dx}x${pos.dy} at ${pos.x},${pos.y}`);
  if (!existsSync(EXE)) {
    throw new Error(`${EXE} doesn't exist: build it first (bun cmd/build.ts -asan -debug)`);
  }

  const skipped = Object.keys(excludedTests);
  console.log(`running ${tests.length} tests, skipping ${skipped.length}: ${skipped.join(", ")}\n`);

  const failures: Failure[] = [];
  const t0 = performance.now();
  for (const [name, fn] of tests) {
    console.log(`\n========== ${name} ==========`);
    try {
      await runTest(name, fn);
    } catch (e) {
      // runTest already timed and recorded it; keep going so one broken test
      // doesn't hide the state of everything after it
      const msg = (e as Error)?.message ?? String(e);
      console.error(`❌ ${msg}`);
      failures.push({ name, msg });
    }
  }

  const elapsed = formatDuration(performance.now() - t0);
  if (failures.length === 0) {
    console.log(`\n✅ run-github-ci: ${tests.length} tests passed in ${elapsed}`);
    return;
  }
  console.log(`\n❌ run-github-ci: ${failures.length} of ${tests.length} tests failed in ${elapsed}:`);
  for (const f of failures) {
    console.log(`  ${f.name}`);
  }
  throw new Error(`${failures.length} test(s) failed: ${failures.map((f) => f.name).join(", ")}`);
}

// names given on the command line, if any: "issue-1195" or "issue-1195,issue-906"
function testsFromArgs(argv: string[]): NamedTest[] | undefined {
  const names = argv
    .slice(2)
    .flatMap((a) => a.split(","))
    .map((a) => a.trim())
    .filter((a) => a && !a.startsWith("-"));
  if (names.length === 0) {
    return undefined;
  }
  const byName = new Map(allTests);
  return names.map((name) => {
    const fn = byName.get(name);
    if (!fn) {
      throw new Error(`no test named '${name}' (it must be registered in run-all.ts)`);
    }
    return [name, fn] as NamedTest;
  });
}

if (import.meta.main) {
  try {
    await testit(testsFromArgs(process.argv));
  } catch (e) {
    console.error(`\n${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
