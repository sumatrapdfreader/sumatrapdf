// Regression test for issue #5941. PDF-XChange Editor v11 renamed its
// executable to PXCEditor.exe and moved clean installations to PDF-XChange.
// The app unit test checks clean v11, upgraded v11, and legacy install paths.

import { runAppUnitTests, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
