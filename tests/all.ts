// Runs every tests/issue-<n>.ts test in sequence, stopping at the first failure.
//
// Each test exports `async function testit()` that throws on failure; this
// imports them all and runs them. Builds the app once up front (unless
// --no-build) so the individual tests don't each rebuild.
//
// Run:  bun tests/all.ts [--no-build] [-silent]
//
// When adding a new test, add it to tests/ as issue-<n>.ts (exporting testit)
// and register it in the `tests` array below.

import { buildApp, formatDuration, isSilentArg, runTest } from "./util.ts";
import { testit as lintCommandIds } from "./lint-command-ids.ts";
import { testit as combiningMarkFirst } from "./combining-mark-first.ts";
import { testit as cmdStartAutoScroll } from "./cmd-start-autoscroll.ts";
import { testit as issue1998 } from "./issue-1998.ts";
import { testit as issue2693 } from "./issue-2693.ts";
import { testit as issue906 } from "./issue-906.ts";
import { testit as issue933 } from "./issue-933.ts";
import { testit as issue3219 } from "./issue-3219.ts";
import { testit as issue3731 } from "./issue-3731.ts";
import { testit as issue3744 } from "./issue-3744.ts";
import { testit as issue4967 } from "./issue-4967.ts";
import { testit as issue5065 } from "./issue-5065.ts";
import { testit as issue5353 } from "./issue-5353.ts";
import { testit as issue5404 } from "./issue-5404.ts";
import { testit as issue5537 } from "./issue-5537.ts";
import { testit as issue5597 } from "./issue-5597.ts";
import { testit as issue5642 } from "./issue-5642.ts";
import { testit as issue5665 } from "./issue-5665.ts";
import { testit as issue5677 } from "./issue-5677.ts";
import { testit as issue5681 } from "./issue-5681.ts";
import { testit as issue1678 } from "./issue-1678.ts";
import { testit as issue5718 } from "./issue-5718.ts";
import { testit as issue5734 } from "./issue-5734.ts";
import { testit as issue5736 } from "./issue-5736.ts";
import { testit as issue5751 } from "./issue-5751.ts";
import { testit as issue5780 } from "./issue-5780.ts";
import { testit as findMatchSelect } from "./issue-find-match-select.ts";
import { testit as issue2252 } from "./issue-2252.ts";
import { testit as issue1201 } from "./issue-1201.ts";
import { testit as parseTipBrackets } from "./parse-tip-brackets.ts";

const tests: [string, () => void | Promise<void>][] = [
  ["lint-command-ids", lintCommandIds],
  ["combining-mark-first", combiningMarkFirst],
  ["cmd-start-autoscroll", cmdStartAutoScroll],
  ["issue-1998", issue1998],
  ["issue-2693", issue2693],
  ["issue-906", issue906],
  ["issue-933", issue933],
  ["issue-3219", issue3219],
  ["issue-3731", issue3731],
  ["issue-3744", issue3744],
  ["issue-4967", issue4967],
  ["issue-5065", issue5065],
  ["issue-5353", issue5353],
  ["issue-5404", issue5404],
  ["issue-5537", issue5537],
  ["issue-5597", issue5597],
  ["issue-5642", issue5642],
  ["issue-5665", issue5665],
  ["issue-5677", issue5677],
  ["issue-5681", issue5681],
  ["issue-1678", issue1678],
  ["issue-5718", issue5718],
  ["issue-5734", issue5734],
  ["issue-5736", issue5736],
  ["issue-5751", issue5751],
  ["issue-5780", issue5780],
  ["issue-find-match-select", findMatchSelect],
  ["issue-2252", issue2252],
  ["issue-1201", issue1201],
  ["parse-tip-brackets", parseTipBrackets],
];

export type AllTestOptions = {
  silent?: boolean;
};

// runs all registered tests in order; throws (stopping) at the first failure
export async function testit(opts?: AllTestOptions): Promise<void> {
  const silent = opts?.silent ?? false;
  const t0 = performance.now();
  for (const [name, fn] of tests) {
    if (!silent) {
      console.log(`\n========== ${name} ==========`);
    }
    await runTest(name, fn, { silent });
  }
  if (!silent) {
    console.log(`\n✅ all ${tests.length} tests passed in ${formatDuration(performance.now() - t0)}`);
  }
}

if (import.meta.main) {
  const silent = isSilentArg();
  if (!process.argv.includes("--no-build")) {
    buildApp({ silent });
  }
  try {
    await testit({ silent });
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
