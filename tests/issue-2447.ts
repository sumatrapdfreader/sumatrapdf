// Regression test for issue #2447. The app unit tests verify that the
// ScrollLineAmount controls the distance of one arrow-key or mouse-wheel line.

import { runAppUnitTests, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
