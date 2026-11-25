import { availableParallelism } from "node:os";
import { EXE, formatDuration, reportParallelTest, usePreparedTestExe } from "./util.ts";
import { disableModifierRelease } from "./winapi.ts";

export async function runParallelTests(scripts: Map<string, string>, silent: boolean): Promise<void> {
  const queue = scripts.entries();
  let failure: unknown;
  async function work(): Promise<void> {
    while (!failure) {
      const next = queue.next();
      if (next.done) {
        return;
      }
      const [name, script] = next.value;
      const t0 = performance.now();
      try {
        const proc = Bun.spawn([process.execPath, import.meta.path, script, EXE, ...process.argv.slice(2)], {
          stdout: "pipe",
          stderr: "pipe",
        });
        const [stdout, stderr, exitCode] = await Promise.all([proc.stdout.text(), proc.stderr.text(), proc.exited]);
        const elapsed = performance.now() - t0;
        reportParallelTest(name, elapsed, exitCode === 0, silent, stdout, stderr);
        if (exitCode !== 0) {
          throw new Error(`${name} failed after ${formatDuration(elapsed)} (exit ${exitCode})`);
        }
      } catch (e) {
        failure ??= e;
      }
    }
  }

  const count = Math.min(scripts.size, availableParallelism());
  await Promise.all(Array.from({ length: count }, work));
  if (failure) throw failure;
}

if (import.meta.main) {
  const [, , script, exe] = process.argv;
  try {
    usePreparedTestExe(exe!);
    disableModifierRelease();
    const { testit } = await import(script!);
    await testit();
    process.exit(0);
  } catch (e) {
    console.error(e instanceof Error ? (e.stack ?? e.message) : String(e));
    process.exit(1);
  }
}
