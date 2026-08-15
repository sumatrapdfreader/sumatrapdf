// Full regular suite: run-almost-all (fast) then the tests whose time is the test.
//
// Run:  bun tests/run-all.ts [--no-build] [-silent]
//
// Register a new test in run-almost-all.ts unless it cannot be made faster
// (print-to-PDF, LaTeX, a measured wait, high-zoom tile settle, a huge
// fixture, or copying the exe next to restrict.ini). Those go in `slowTests`.

import { formatDuration, runNamedTests, runSuiteMain, type NamedTest, type SuiteOptions } from "./util.ts";
import { tests as almostAllTests, testit as runAlmostAll } from "./run-almost-all.ts";
import { testit as issue4967 } from "./issue-4967.ts";
import { testit as issue5065 } from "./issue-5065.ts";
import { testit as issue5353 } from "./issue-5353.ts";
import { testit as issue5040 } from "./issue-5040.ts";
import { testit as reloadDebounce } from "./reload-debounce.ts";
import { testit as issue2693 } from "./issue-2693.ts";
import { testit as issue5865 } from "./issue-5865.ts";
import { testit as issue5918 } from "./issue-5918.ts";
import { testit as ghsaCrhmW5qrWjj4 } from "./security-ghsa-crhm-w5qr-wjj4.ts";
import { testit as issue1195 } from "./issue-1195.ts";

export const slowTests: NamedTest[] = [
  ["issue-4967", issue4967],
  ["issue-5065", issue5065],
  ["issue-5353", issue5353],
  ["issue-5040", issue5040],
  ["reload-debounce", reloadDebounce],
  ["issue-2693", issue2693],
  ["issue-5865", issue5865],
  ["issue-5918", issue5918],
  ["security-ghsa-crhm-w5qr-wjj4", ghsaCrhmW5qrWjj4],
  ["issue-1195", issue1195],
];

export const tests: NamedTest[] = [...almostAllTests, ...slowTests];

export type AllTestOptions = SuiteOptions;

export async function testit(opts?: AllTestOptions): Promise<void> {
  const keep = opts?.keepTestTimes ?? false;
  const silent = opts?.silent ?? false;
  const t0 = performance.now();
  await runAlmostAll({ silent, keepTestTimes: keep, summary: false });
  if (!silent) {
    console.log(`\n========== slow ==========`);
  }
  await runNamedTests(slowTests, { silent, keepTestTimes: true, summary: false });
  if (!silent) {
    console.log(`\n✅ all ${tests.length} tests passed in ${formatDuration(performance.now() - t0)}`);
  }
}

if (import.meta.main) {
  await runSuiteMain(testit);
}
