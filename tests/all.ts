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
import { testit as lintMingwSources } from "./lint-mingw-sources.ts";
import { testit as combiningMarkFirst } from "./combining-mark-first.ts";
import { testit as issue1106 } from "./issue-1106.ts";
import { testit as issue1438 } from "./issue-1438.ts";
import { testit as issue1136 } from "./issue-1136.ts";
import { testit as issue1699 } from "./issue-1699.ts";
import { testit as issue1998 } from "./issue-1998.ts";
import { testit as issue2199 } from "./issue-2199.ts";
import { testit as issue2693 } from "./issue-2693.ts";
import { testit as issue906 } from "./issue-906.ts";
import { testit as issue933 } from "./issue-933.ts";
import { testit as issue3219 } from "./issue-3219.ts";
import { testit as issue3560 } from "./issue-3560.ts";
import { testit as issue3591 } from "./issue-3591.ts";
import { testit as issue3769 } from "./issue-3769.ts";
import { testit as issue3731 } from "./issue-3731.ts";
import { testit as issue3744 } from "./issue-3744.ts";
import { testit as issue4967 } from "./issue-4967.ts";
import { testit as issue4973 } from "./issue-4973.ts";
import { testit as issue5065 } from "./issue-5065.ts";
import { testit as issue1195 } from "./issue-1195.ts";
import { testit as issue5095 } from "./issue-5095.ts";
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
import { testit as issue5529 } from "./issue-5529.ts";
import { testit as issue2629 } from "./issue-2629.ts";
import { testit as issue4684 } from "./issue-4684.ts";
import { testit as issue5922 } from "./issue-5922.ts";
import { testit as issue5924 } from "./issue-5924.ts";
import { testit as issue5926 } from "./issue-5926.ts";
import { testit as issue5040 } from "./issue-5040.ts";
import { testit as issue1724 } from "./issue-1724.ts";
import { testit as issue1914 } from "./issue-1914.ts";
import { testit as issue2799 } from "./issue-2799.ts";
import { testit as findMatchSelect } from "./issue-find-match-select.ts";
import { testit as findResultsSorted } from "./find-results-sorted.ts";
import { testit as issue5874 } from "./issue-5874.ts";
import { testit as issue2252 } from "./issue-2252.ts";
import { testit as issue2254 } from "./issue-2254.ts";
import { testit as issue1201 } from "./issue-1201.ts";
import { testit as issue1189 } from "./issue-1189.ts";
import { testit as issue5792 } from "./issue-5792.ts";
import { testit as issue4576 } from "./issue-4576.ts";
import { testit as issue5834 } from "./issue-5834.ts";
import { testit as issue5840 } from "./issue-5840.ts";
import { testit as issue5842 } from "./issue-5842.ts";
import { testit as issue5845 } from "./issue-5845.ts";
import { testit as issue5846 } from "./issue-5846.ts";
import { testit as issue5850 } from "./issue-5850.ts";
import { testit as issue5865 } from "./issue-5865.ts";
import { testit as issue5866 } from "./issue-5866.ts";
import { testit as issue5867 } from "./issue-5867.ts";
import { testit as issue5868 } from "./issue-5868.ts";
import { testit as issue5870 } from "./issue-5870.ts";
import { testit as issue5870ListDirs } from "./issue-5870-list-dirs.ts";
import { testit as issue5869 } from "./issue-5869.ts";
import { testit as issue5871 } from "./issue-5871.ts";
import { testit as issue5873 } from "./issue-5873.ts";
import { testit as issue5875 } from "./issue-5875.ts";
import { testit as issue5881 } from "./issue-5881.ts";
import { testit as rectSelectionDrag } from "./rect-selection-drag.ts";
import { testit as issue5882 } from "./issue-5882.ts";
import { testit as issue5899 } from "./issue-5899.ts";
import { testit as issue5907 } from "./issue-5907.ts";
import { testit as issue5917 } from "./issue-5917.ts";
import { testit as issue5918 } from "./issue-5918.ts";
import { testit as reloadDebounce } from "./reload-debounce.ts";
import { testit as parseTipBrackets } from "./parse-tip-brackets.ts";
import { testit as ghsaP2ph2rvmQ37m } from "./security-ghsa-p2ph-2rvm-q37m.ts";
import { testit as ghsaCrhmW5qrWjj4 } from "./security-ghsa-crhm-w5qr-wjj4.ts";
import { testit as ghsaJf4vRw66J4w2 } from "./security-ghsa-jf4v-rw66-j4w2.ts";
import { testit as issue5934 } from "./issue-5934.ts";

const tests: [string, () => void | Promise<void>][] = [
  ["lint-command-ids", lintCommandIds],
  ["lint-mingw-sources", lintMingwSources],
  ["combining-mark-first", combiningMarkFirst],
  ["issue-1106", issue1106],
  ["issue-1438", issue1438],
  ["issue-1136", issue1136],
  ["issue-1699", issue1699],
  ["issue-1998", issue1998],
  ["issue-2199", issue2199],
  ["issue-2693", issue2693],
  ["issue-906", issue906],
  ["issue-933", issue933],
  ["issue-3219", issue3219],
  ["issue-3560", issue3560],
  ["issue-3591", issue3591],
  ["issue-3769", issue3769],
  ["issue-3731", issue3731],
  ["issue-3744", issue3744],
  ["issue-4967", issue4967],
  ["issue-4973", issue4973],
  ["issue-5065", issue5065],
  ["issue-1195", issue1195],
  ["issue-5095", issue5095],
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
  ["issue-5529", issue5529],
  ["issue-2629", issue2629],
  ["issue-4684", issue4684],
  ["issue-5922", issue5922],
  ["issue-5924", issue5924],
  ["issue-5926", issue5926],
  ["issue-5040", issue5040],
  ["issue-1724", issue1724],
  ["issue-1914", issue1914],
  ["issue-2799", issue2799],
  ["issue-find-match-select", findMatchSelect],
  ["find-results-sorted", findResultsSorted],
  ["issue-5874", issue5874],
  ["issue-2252", issue2252],
  ["issue-2254", issue2254],
  ["issue-1201", issue1201],
  ["issue-1189", issue1189],
  ["issue-5792", issue5792],
  ["issue-4576", issue4576],
  ["issue-5834", issue5834],
  ["issue-5840", issue5840],
  ["issue-5842", issue5842],
  ["issue-5845", issue5845],
  ["issue-5846", issue5846],
  ["issue-5850", issue5850],
  ["issue-5865", issue5865],
  ["issue-5866", issue5866],
  ["issue-5867", issue5867],
  ["issue-5868", issue5868],
  ["issue-5870", issue5870],
  ["issue-5870-list-dirs", issue5870ListDirs],
  ["issue-5869", issue5869],
  ["issue-5871", issue5871],
  ["issue-5873", issue5873],
  ["issue-5875", issue5875],
  ["issue-5881", issue5881],
  ["rect-selection-drag", rectSelectionDrag],
  ["issue-5882", issue5882],
  ["issue-5899", issue5899],
  ["issue-5907", issue5907],
  ["issue-5917", issue5917],
  ["issue-5918", issue5918],
  ["reload-debounce", reloadDebounce],
  ["parse-tip-brackets", parseTipBrackets],
  ["security-ghsa-p2ph-2rvm-q37m", ghsaP2ph2rvmQ37m],
  ["security-ghsa-crhm-w5qr-wjj4", ghsaCrhmW5qrWjj4],
  ["security-ghsa-jf4v-rw66-j4w2", ghsaJf4vRw66J4w2],
  ["issue-5934", issue5934],
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
