// Runs every tests/issue-<n>.ts test in sequence, stopping at the first failure.
//
// Each test exports `async function testit()` that throws on failure; this
// imports them all and runs them. Builds the app once up front (unless
// --no-build) so the individual tests don't each rebuild.
//
// Run:  bun tests/all.ts [--no-build]
//
// When adding a new test, add it to tests/ as issue-<n>.ts (exporting testit)
// and register it in the `tests` array below.

import { buildApp } from "./util.ts";
import { testit as issue906 } from "./issue-906.ts";
import { testit as issue4967 } from "./issue-4967.ts";
import { testit as issue5065 } from "./issue-5065.ts";
import { testit as issue5353 } from "./issue-5353.ts";
import { testit as issue5537 } from "./issue-5537.ts";
import { testit as issue5597 } from "./issue-5597.ts";
import { testit as issue5642 } from "./issue-5642.ts";
import { testit as issue5665 } from "./issue-5665.ts";
import { testit as issue5677 } from "./issue-5677.ts";
import { testit as issue5681 } from "./issue-5681.ts";
import { testit as issueChmLzx } from "./issue-chm-lzx.ts";

const tests: [string, () => void | Promise<void>][] = [
  ["issue-906", issue906],
  ["issue-4967", issue4967],
  ["issue-5065", issue5065],
  ["issue-5353", issue5353],
  ["issue-5537", issue5537],
  ["issue-5597", issue5597],
  ["issue-5642", issue5642],
  ["issue-5665", issue5665],
  ["issue-5677", issue5677],
  ["issue-5681", issue5681],
  ["issue-chm-lzx", issueChmLzx],
];

// runs all registered tests in order; throws (stopping) at the first failure
export async function testit(): Promise<void> {
  for (const [name, fn] of tests) {
    console.log(`\n========== ${name} ==========`);
    try {
      await fn();
    } catch (e) {
      throw new Error(`${name} failed: ${(e as Error)?.message ?? e}`);
    }
  }
  console.log(`\n✅ all ${tests.length} tests passed`);
}

if (import.meta.main) {
  if (!process.argv.includes("--no-build")) {
    buildApp();
  }
  try {
    await testit();
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
