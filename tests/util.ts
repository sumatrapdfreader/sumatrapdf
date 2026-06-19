// Shared helpers for tests/. See agents.md "Writing tests".
//
// Each tests/issue-<n>.ts exports `async function testit()` that runs the test
// and THROWS on failure (returns normally on success). It does NOT build the app
// or call process.exit -- that's the runner's job, so tests compose in all.ts.

import { mkdirSync } from "node:fs";
import { join } from "node:path";

export const ROOT = join(import.meta.dir, "..");
export const EXE = join(ROOT, "out", "dbg64", "SumatraPDF-dll.exe");

// directory for temporary / scratch files produced by tests. It's gitignored
// (tests/tmp/), so tests must write their runtime output here, never directly
// into tests/. Use tmpPath() to get a path inside it (dir created on demand).
export const TMP_DIR = join(import.meta.dir, "tmp");

export function tmpPath(name: string): string {
  mkdirSync(TMP_DIR, { recursive: true });
  return join(TMP_DIR, name);
}

// format a duration in ms for test output (e.g. 34.3ms, 2.3s, 3m 2.3s)
export function formatDuration(ms: number): string {
  if (ms < 1000) {
    return `${ms.toFixed(1)}ms`;
  }
  const sec = ms / 1000;
  if (sec < 60) {
    return `${sec.toFixed(1)}s`;
  }
  const min = Math.floor(sec / 60);
  const remSec = sec - min * 60;
  return `${min}m ${remSec.toFixed(1)}s`;
}

// run one test and print pass/fail timing
export async function runTest(name: string, fn: () => void | Promise<void>): Promise<void> {
  const t0 = performance.now();
  try {
    await fn();
    console.log(`✅ ${name} passed in ${formatDuration(performance.now() - t0)}`);
  } catch (e) {
    const msg = (e as Error)?.message ?? e;
    throw new Error(`${name} failed after ${formatDuration(performance.now() - t0)}: ${msg}`);
  }
}

// build SumatraPDF-dll.exe the same way cmd/build.ts does
export function buildApp(): void {
  console.log("• building SumatraPDF-dll.exe (cmd/build.ts) ...");
  const p = Bun.spawnSync({ cmd: ["bun", join(ROOT, "cmd", "build.ts")], cwd: ROOT, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    throw new Error("build failed");
  }
}

// entry point for running a single test file directly:
//   bun tests/issue-<n>.ts [--no-build]
// builds (unless --no-build), runs testit(), exits 0 on pass / 1 on failure.
export async function runStandalone(testit: () => void | Promise<void>, name?: string): Promise<void> {
  const label =
    name ??
    (process.argv[1] ?? "test")
      .replace(/\\/g, "/")
      .split("/")
      .pop()!
      .replace(/\.ts$/, "");
  try {
    if (!process.argv.includes("--no-build")) {
      buildApp();
    }
    await runTest(label, testit);
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
