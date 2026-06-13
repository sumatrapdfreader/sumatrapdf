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
import { testit as issue5537 } from "./issue-5537.ts";
import { testit as issue5597 } from "./issue-5597.ts";
import { testit as issue5633 } from "./issue-5633.ts";
import { testit as issue5677 } from "./issue-5677.ts";

const tests: [string, () => void | Promise<void>][] = [
  ["issue-5537", issue5537],
  ["issue-5597", issue5597],
  ["issue-5633", issue5633],
  ["issue-5677", issue5677],
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
