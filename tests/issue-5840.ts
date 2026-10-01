// Regression test for issue #5840. The app unit tests replay selection
// rectangle aggregation across dehyphenated visual-line boundaries.

import { runAppUnitTests, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
