// Regression: ParseTip must not hang on '[' in plain text (e.g. loading
// notifications for files like "Apocalypse Bringer Mynoghra_01 [CIW].pdf").
// Implemented in AppUnitTests.cpp, run via -unit-tests (debug builds only).

import { EXE, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  const proc = Bun.spawn([EXE, "-unit-tests"], { stdout: "pipe", stderr: "pipe" });
  const [stdout, stderr, exitCode] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  const out = (stdout + stderr).trim();
  if (exitCode !== 0) {
    throw new Error(`parse-tip-brackets: -unit-tests failed (exit ${exitCode}):\n${out}`);
  }
  console.log(out || "parse-tip-brackets: -unit-tests passed");
}

if (import.meta.main) {
  await runStandalone(testit, { noBuild: false });
}