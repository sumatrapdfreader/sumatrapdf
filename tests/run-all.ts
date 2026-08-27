// Full regular suite: run-almost-all (fast) then the tests whose time is the test.
//
// Run:  bun tests/run-all.ts [--no-build] [-silent] [-exe <SumatraPDF.exe>]
//
// Register a new test in run-almost-all.ts unless it cannot be made faster
// (print-to-PDF, LaTeX, a measured wait, high-zoom tile settle, a huge
// fixture, or copying the exe next to restrict.ini). Those go in `slowTests`.

import {
  formatDuration,
  runNamedTests,
  runSuiteMain,
  startSuiteProgress,
  type NamedTest,
  type SuiteOptions,
} from "./util.ts";
import { tests as almostAllTests, testit as runAlmostAll } from "./run-almost-all.ts";
import { testit as issue5842 } from "./issue-5842.ts";
import { testit as issue6003 } from "./issue-6003.ts";
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
import { testit as issue5870ListDirs } from "./issue-5870-list-dirs.ts";
import { testit as issue1324 } from "./issue-1324.ts";
import { testit as layoutCallback } from "./layout-callback.ts";
import { testit as issue5069 } from "./issue-5069.ts";
import { testit as issue5934 } from "./issue-5934.ts";
import { testit as issue5917 } from "./issue-5917.ts";
import { testit as issue2022 } from "./issue-2022.ts";
import { testit as issue1203 } from "./issue-1203.ts";
import { testit as issue5792 } from "./issue-5792.ts";
import { testit as issue5972 } from "./issue-5972.ts";
import { testit as annotFilterToolbar } from "./annot-filter-toolbar.ts";

// The slowest of the regular tests. They still run here and in the daily CI
// suite, just not in run-pre-release: a couple of seconds each is nothing on
// its own, but a pre-release build waits for the whole suite before it uploads,
// and these tests were a sixth of its run.
export const notInPreReleaseTests: NamedTest[] = [
  ["annot-filter-toolbar", annotFilterToolbar],
  ["issue-5972", issue5972],
  ["issue-5870-list-dirs", issue5870ListDirs],
  ["issue-1324", issue1324],
  ["layout-callback", layoutCallback],
  ["issue-5069", issue5069],
  ["issue-5934", issue5934],
  ["issue-5917", issue5917],
  ["issue-2022", issue2022],
  ["issue-1203", issue1203],
  ["issue-5792", issue5792],
];

export const slowTests: NamedTest[] = [
  // WebView TOC
  ["issue-5842", issue5842],
  ["issue-6003", issue6003],
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

export const tests: NamedTest[] = [...almostAllTests, ...notInPreReleaseTests, ...slowTests];

export type AllTestOptions = SuiteOptions;

export async function testit(opts?: AllTestOptions): Promise<void> {
  const keep = opts?.keepTestTimes ?? false;
  const silent = opts?.silent ?? false;
  const t0 = performance.now();
  startSuiteProgress(tests.length);
  await runAlmostAll({ silent, keepTestTimes: keep, summary: false });
  if (!silent) {
    console.log(`\n========== slow ==========`);
  }
  await runNamedTests([...notInPreReleaseTests, ...slowTests], {
    silent,
    keepTestTimes: true,
    summary: false,
  });
  console.log(`\n✅ all ${tests.length} tests passed in ${formatDuration(performance.now() - t0)}`);
}

if (import.meta.main) {
  await runSuiteMain(testit);
}
