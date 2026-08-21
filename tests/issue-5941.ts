// Regression test for issue #5941. PDF-XChange Editor v11 renamed its
// executable to PXCEditor.exe and moved clean installations to PDF-XChange.
// The app unit test checks clean v11, upgraded v11, and legacy install paths.

import { EXE, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  const proc = Bun.spawn([EXE, "-unit-tests"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  const [stdout, stderr, exitCode] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  const output = (stdout + stderr).trim();
  if (exitCode !== 0) {
    throw new Error(`#5941 app unit tests failed (exit ${exitCode}):\n${output}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
