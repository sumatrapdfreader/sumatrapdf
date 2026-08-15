// Pre-release suite: the fast regular tests plus LaTeX / SyncTeX.
//
// Run:  bun tests/run-pre-release.ts [--no-build] [-silent]

import { formatDuration, resetTestTimes, runSuiteMain, runTest, type SuiteOptions } from "./util.ts";
import { testit as runAlmostAll } from "./run-almost-all.ts";
import { testit as latexTests } from "./latex.ts";

export async function testit(opts?: SuiteOptions): Promise<void> {
  const silent = opts?.silent ?? false;
  const t0 = performance.now();
  resetTestTimes();
  await runAlmostAll({ silent, keepTestTimes: true, summary: false });

  if (!silent) {
    console.log("\n========== latex ==========");
  }
  await runTest("latex", latexTests, { silent });

  if (!silent) {
    console.log(`\n✅ pre-release checks passed (run-almost-all + latex) in ${formatDuration(performance.now() - t0)}`);
  }
}

if (import.meta.main) {
  await runSuiteMain(testit);
}
