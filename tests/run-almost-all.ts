// Fast regular tests: everything except cases whose time is the test
// (print-to-PDF, LaTeX, debounce/CPU/scroll windows, high-zoom tile settle,
// 400-file HTML TOC, restrict.ini exe copies). Those live in run-all.ts.
//
// Order groups tests that can share a Sumatra session (default -for-testing +
// quadrant -window-pos + -dbg-control) so a later shared-process runner can
// keep one instance. Isolated tests (custom -appdata, session restore, their
// own window placement) come after.
//
// Run:  bun tests/run-almost-all.ts [--no-build] [-silent]
//
// Register a new fast test here. Inherently-slow ones go in tests/run-all.ts.

import { runNamedTests, runSuiteMain, type NamedTest, type SuiteOptions } from "./util.ts";
import { testit as lintCommandIds } from "./lint-command-ids.ts";
import { testit as lintMingwSources } from "./lint-mingw-sources.ts";
import { testit as buildCli } from "./build-cli.ts";
import { testit as combiningMarkFirst } from "./combining-mark-first.ts";
import { testit as parseTipBrackets } from "./parse-tip-brackets.ts";
import { testit as issue5840 } from "./issue-5840.ts";
import { testit as issue5846 } from "./issue-5846.ts";
import { testit as issue5941 } from "./issue-5941.ts";
import { testit as issue2447 } from "./issue-2447.ts";
import { testit as issue476 } from "./issue-476.ts";
import { testit as issue5875 } from "./issue-5875.ts";
import { testit as issue3769 } from "./issue-3769.ts";
import { testit as issue3744 } from "./issue-3744.ts";
import { testit as issue4973 } from "./issue-4973.ts";
import { testit as issue5329 } from "./issue-5329.ts";
import { testit as issue5718 } from "./issue-5718.ts";
import { testit as issue5734 } from "./issue-5734.ts";
import { testit as issue5736 } from "./issue-5736.ts";
import { testit as issue5529 } from "./issue-5529.ts";
import { testit as issue2629 } from "./issue-2629.ts";
import { testit as issue4684 } from "./issue-4684.ts";
import { testit as issue5922 } from "./issue-5922.ts";
import { testit as issue5924 } from "./issue-5924.ts";
import { testit as issue5926 } from "./issue-5926.ts";
import { testit as issue1914 } from "./issue-1914.ts";
import { testit as issue2799 } from "./issue-2799.ts";
import { testit as findMatchSelect } from "./issue-find-match-select.ts";
import { testit as findResultsSorted } from "./find-results-sorted.ts";
import { testit as issue5874 } from "./issue-5874.ts";
import { testit as issue2252 } from "./issue-2252.ts";
import { testit as issue5834 } from "./issue-5834.ts";
import { testit as issue5842 } from "./issue-5842.ts";
import { testit as issue5869 } from "./issue-5869.ts";
import { testit as issue5881 } from "./issue-5881.ts";
import { testit as rectSelectionDrag } from "./rect-selection-drag.ts";
import { testit as issue5938 } from "./issue-5938.ts";
import { testit as issue2873 } from "./issue-2873.ts";
import { testit as issue5937 } from "./issue-5937.ts";
import { testit as issue933 } from "./issue-933.ts";
import { testit as issue3219 } from "./issue-3219.ts";
import { testit as issue5404 } from "./issue-5404.ts";
import { testit as issue5537 } from "./issue-5537.ts";
import { testit as issue5597 } from "./issue-5597.ts";
import { testit as issue5642 } from "./issue-5642.ts";
import { testit as issue5665 } from "./issue-5665.ts";
import { testit as issue5677 } from "./issue-5677.ts";
import { testit as issue5681 } from "./issue-5681.ts";
import { testit as issue1678 } from "./issue-1678.ts";
import { testit as issue1724 } from "./issue-1724.ts";
import { testit as issue1201 } from "./issue-1201.ts";
import { testit as issue1189 } from "./issue-1189.ts";
import { testit as issue5871 } from "./issue-5871.ts";
import { testit as issue5873 } from "./issue-5873.ts";
import { testit as ghsaJf4vRw66J4w2 } from "./security-ghsa-jf4v-rw66-j4w2.ts";
import { testit as issue5317 } from "./issue-5317.ts";
import { testit as issue5694 } from "./issue-5694.ts";
import { testit as issue3731 } from "./issue-3731.ts";
import { testit as issue5095 } from "./issue-5095.ts";
import { testit as issue5751 } from "./issue-5751.ts";
import { testit as issue5780 } from "./issue-5780.ts";
import { testit as issue2254 } from "./issue-2254.ts";
import { testit as issue5950 } from "./issue-5950.ts";
import { testit as issue5792 } from "./issue-5792.ts";
import { testit as issue5845 } from "./issue-5845.ts";
import { testit as issue5870 } from "./issue-5870.ts";
import { testit as issue5882 } from "./issue-5882.ts";
import { testit as issue5917 } from "./issue-5917.ts";
import { testit as ghsaP2ph2rvmQ37m } from "./security-ghsa-p2ph-2rvm-q37m.ts";
import { testit as issue4753 } from "./issue-4753.ts";
import { testit as issue4055 } from "./issue-4055.ts";
import { testit as issue2165 } from "./issue-2165.ts";
import { testit as issue1203 } from "./issue-1203.ts";
import { testit as issue3472 } from "./issue-3472.ts";
import { testit as pdfOnlyMenuItems } from "./pdf-only-menu-items.ts";
import { testit as issue2258 } from "./issue-2258.ts";
import { testit as issue2737 } from "./issue-2737.ts";
import { testit as issue1106 } from "./issue-1106.ts";
import { testit as issue1438 } from "./issue-1438.ts";
import { testit as issue1136 } from "./issue-1136.ts";
import { testit as issue1699 } from "./issue-1699.ts";
import { testit as issue1998 } from "./issue-1998.ts";
import { testit as issue2199 } from "./issue-2199.ts";
import { testit as issue906 } from "./issue-906.ts";
import { testit as issue3560 } from "./issue-3560.ts";
import { testit as issue3591 } from "./issue-3591.ts";
import { testit as issue4576 } from "./issue-4576.ts";
import { testit as issue5850 } from "./issue-5850.ts";
import { testit as issue5866 } from "./issue-5866.ts";
import { testit as issue5867 } from "./issue-5867.ts";
import { testit as issue5868 } from "./issue-5868.ts";
import { testit as issue5870ListDirs } from "./issue-5870-list-dirs.ts";
import { testit as issue5899 } from "./issue-5899.ts";
import { testit as issue5907 } from "./issue-5907.ts";
import { testit as issue5934 } from "./issue-5934.ts";

export const tests: NamedTest[] = [
  // --- no Sumatra process -------------------------------------------------
  ["lint-command-ids", lintCommandIds],
  ["lint-mingw-sources", lintMingwSources],
  ["build-cli", buildCli],
  ["parse-tip-brackets", parseTipBrackets],
  ["combining-mark-first", combiningMarkFirst],
  ["issue-5840", issue5840],
  ["issue-5846", issue5846],
  ["issue-5941", issue5941],
  ["issue-2447", issue2447],
  ["issue-476", issue476],
  ["issue-5875", issue5875],

  // --- default session: -for-testing + quadrant window + -dbg-control ----
  ["issue-3769", issue3769],
  ["issue-4973", issue4973],
  ["issue-5329", issue5329],
  ["issue-5718", issue5718],
  ["issue-5734", issue5734],
  ["issue-5736", issue5736],
  ["issue-5529", issue5529],
  ["issue-2629", issue2629],
  ["issue-4684", issue4684],
  ["issue-5922", issue5922],
  ["issue-5924", issue5924],
  ["issue-5926", issue5926],
  ["issue-1914", issue1914],
  ["issue-2799", issue2799],
  ["issue-find-match-select", findMatchSelect],
  ["find-results-sorted", findResultsSorted],
  ["issue-5874", issue5874],
  ["issue-2252", issue2252],
  ["issue-5834", issue5834],
  ["issue-5842", issue5842],
  ["issue-5869", issue5869],
  ["issue-5881", issue5881],
  ["rect-selection-drag", rectSelectionDrag],
  ["issue-5938", issue5938],
  ["issue-2873", issue2873],
  ["issue-5937", issue5937],
  ["issue-933", issue933],
  ["issue-3219", issue3219],
  ["issue-5404", issue5404],
  ["issue-5537", issue5537],
  ["issue-5597", issue5597],
  ["issue-5642", issue5642],
  ["issue-5665", issue5665],
  ["issue-5677", issue5677],
  ["issue-5681", issue5681],
  ["issue-1678", issue1678],
  ["issue-1724", issue1724],
  ["issue-1201", issue1201],
  ["issue-1189", issue1189],
  ["issue-5871", issue5871],
  ["issue-5873", issue5873],
  ["security-ghsa-jf4v-rw66-j4w2", ghsaJf4vRw66J4w2],
  ["issue-5317", issue5317],
  ["issue-5694", issue5694],
  ["issue-1699", issue1699],
  ["issue-2254", issue2254],
  ["issue-5950", issue5950],
  ["issue-5792", issue5792],
  ["issue-3472", issue3472],
  ["pdf-only-menu-items", pdfOnlyMenuItems],
  ["security-ghsa-p2ph-2rvm-q37m", ghsaP2ph2rvmQ37m],
  ["issue-5780", issue5780],
  ["issue-5845", issue5845],
  ["issue-5870", issue5870],
  ["issue-5934", issue5934],

  // --- isolated session: -appdata, saveSettings, or own window placement -
  ["issue-3744", issue3744],
  ["issue-5095", issue5095],
  ["issue-3731", issue3731],
  ["issue-5751", issue5751],
  ["issue-5882", issue5882],
  ["issue-5917", issue5917],
  ["issue-4753", issue4753],
  ["issue-4055", issue4055],
  ["issue-2165", issue2165],
  ["issue-1203", issue1203],
  ["issue-2258", issue2258],
  ["issue-2737", issue2737],
  ["issue-1106", issue1106],
  ["issue-1438", issue1438],
  ["issue-1136", issue1136],
  ["issue-1998", issue1998],
  ["issue-2199", issue2199],
  ["issue-906", issue906],
  ["issue-3560", issue3560],
  ["issue-3591", issue3591],
  ["issue-4576", issue4576],
  ["issue-5850", issue5850],
  ["issue-5866", issue5866],
  ["issue-5867", issue5867],
  ["issue-5868", issue5868],
  ["issue-5870-list-dirs", issue5870ListDirs],
  ["issue-5899", issue5899],
  ["issue-5907", issue5907],
];

export async function testit(opts?: SuiteOptions): Promise<void> {
  await runNamedTests(tests, { heading: "run-almost-all", ...opts });
}

if (import.meta.main) {
  await runSuiteMain(testit);
}
