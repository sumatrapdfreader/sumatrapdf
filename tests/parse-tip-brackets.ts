// Regression: ParseTip must not hang on '[' in plain text (e.g. loading
// notifications for files like "Apocalypse Bringer Mynoghra_01 [CIW].pdf").
// Implemented in src/tests/Sumatra_ut.cpp, run via -unit-tests (debug builds only).

import { runAppUnitTests, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  await runAppUnitTests();
}

if (import.meta.main) {
  await runStandalone(testit);
}
