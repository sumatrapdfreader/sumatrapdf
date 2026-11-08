// Runs the app's unit tests (debug builds, -unit-tests). They hold the
// regression tests for, among others, #476, #2447, #5840, #5846, #5941 and
// ParseTip with '[' in plain text.
//
// Run: bun tests/app-unit-tests.ts [--no-build]

import { runAppUnitTests, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
