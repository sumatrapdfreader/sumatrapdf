// Regression test for issue #476. The app unit tests verify that valid ebook
// line-spacing multipliers generate overriding MuPDF user CSS.

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
    throw new Error(`#476 app unit tests failed (exit ${exitCode}):\n${output}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
