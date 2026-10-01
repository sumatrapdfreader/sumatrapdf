// Regression test for issue #476. The app unit tests verify that valid ebook
// line-spacing multipliers generate overriding MuPDF user CSS.

import { runAppUnitTests, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
