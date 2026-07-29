// Shared helpers for tests/. See agents.md "Writing tests".
//
// Each tests/issue-<n>.ts exports `async function testit()` that runs the test
// and THROWS on failure (returns normally on success). It does NOT build the app
// or call process.exit -- that's the runner's job, so tests compose in all.ts.

import { mkdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const ROOT = join(import.meta.dir, "..");
export const EXE = join(ROOT, "out", "dbg64", "SumatraPDF.exe");

// Command ids (sent with WM_COMMAND) live in src/Commands.h, but they're
// generated and renumber whenever a command is added or removed -- so tests must
// never hardcode the integer. Look it up by name at runtime instead, so a test
// keeps targeting the right command after the enum shifts.
let cmdIdCache: Map<string, number> | null = null;
export function cmdId(name: string): number {
  if (!cmdIdCache) {
    cmdIdCache = new Map();
    const src = readFileSync(join(ROOT, "src", "Commands.h"), "utf8");
    const re = /\b(Cmd\w+)\s*=\s*(\d+)\b/g;
    let m: RegExpExecArray | null;
    while ((m = re.exec(src)) !== null) {
      cmdIdCache.set(m[1], parseInt(m[2], 10));
    }
  }
  const id = cmdIdCache.get(name);
  if (id === undefined) {
    throw new Error(`cmdId: '${name}' not found in src/Commands.h`);
  }
  return id;
}

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

export type RunTestOptions = {
  silent?: boolean;
};

function muteConsole(): () => void {
  const log = console.log;
  const error = console.error;
  console.log = () => {};
  console.error = () => {};
  return () => {
    console.log = log;
    console.error = error;
  };
}

// run one test and print pass/fail timing
export async function runTest(
  name: string,
  fn: () => void | Promise<void>,
  opts?: RunTestOptions,
): Promise<void> {
  const silent = opts?.silent ?? false;
  const t0 = performance.now();
  const unmute = silent ? muteConsole() : () => {};
  try {
    await fn();
    unmute();
    const elapsed = formatDuration(performance.now() - t0);
    if (silent) {
      console.log(`== ${name} in ${elapsed}`);
    } else {
      console.log(`✅ ${name} passed in ${elapsed}`);
    }
  } catch (e) {
    unmute();
    const msg = (e as Error)?.message ?? e;
    throw new Error(`${name} failed after ${formatDuration(performance.now() - t0)}: ${msg}`);
  }
}

export function isSilentArg(argv: string[] = process.argv): boolean {
  return argv.includes("-silent") || argv.includes("--silent");
}

// build SumatraPDF.exe the same way cmd/build.ts does
export function buildApp(opts?: { silent?: boolean }): void {
  if (!opts?.silent) {
    console.log("• building SumatraPDF.exe (cmd/build.ts) ...");
  }
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
