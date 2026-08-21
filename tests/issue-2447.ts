// Regression test for issue #2447. The app unit tests verify that the
// ScrollLineAmount controls the distance of one arrow-key or mouse-wheel line.

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
    throw new Error(`#2447 app unit tests failed (exit ${exitCode}):\n${output}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
