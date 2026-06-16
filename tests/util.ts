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
export async function runStandalone(testit: () => void | Promise<void>): Promise<void> {
  try {
    if (!process.argv.includes("--no-build")) {
      buildApp();
    }
    await testit();
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
