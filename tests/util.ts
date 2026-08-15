// Shared helpers for tests/. See agents.md "Writing tests".
//
// Each tests/issue-<n>.ts exports `async function testit()` that runs the test
// and THROWS on failure (returns normally on success). It does NOT build the app
// or call process.exit -- that's the runner's job, so tests compose in
// run-almost-all.ts / run-all.ts.

import { appendFileSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";

export const ROOT = join(import.meta.dir, "..");
export const EXE = join(ROOT, "out", "dbg64", "SumatraPDF.exe");

// Extract page text via the debug -extract-text harness (hex-encoded UTF-8).
// The GUI exe's stdout often does not reach a Bun pipe on Windows; PowerShell
// (a console app) relays it. pageNo -1 means all pages (same as the flag).
// Newlines in the harness are encoded as '_' (0x5f); those become real '\n'.
// Throws if no "text on page" line appears (missing DEBUG build, bad path, …).
export function extractPageText(file: string, pageNo: number = -1): string {
  const psCmd = `& '${EXE}' -for-testing -extract-text ${pageNo} '${file}' 2>&1 | Out-String -Width 100000`;
  const p = Bun.spawnSync(["powershell", "-NoProfile", "-Command", psCmd]);
  const raw = p.stdout.toString() + p.stderr.toString();
  let all = "";
  let nPages = 0;
  for (const m of raw.matchAll(/text on page \d+: '([0-9a-f ]*)'/g)) {
    nPages++;
    const hex = m[1].trim();
    if (!hex) {
      continue;
    }
    const bytes = hex.split(/\s+/).map((h) => parseInt(h, 16));
    all += Buffer.from(bytes).toString("utf8");
  }
  if (nPages === 0) {
    throw new Error(
      `extractPageText: no text from ${file} (debug build? -extract-text is DEBUG-only). ` +
        `out=${raw.slice(0, 400).replace(/\s+/g, " ")}`,
    );
  }
  // '_' is the harness's stand-in for newline
  return all.split("_").join("\n");
}

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

// A valid one-page PDF with no content, for tests that only need a document the
// app can open (a link target, a tab to switch to, ...). The xref offsets are
// computed, so mupdf loads it without running its repair pass. For a PDF with
// actual text on the page see makeTextPdf in tests/issue-5922.ts.
export function makeMinimalPdf(title: string): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    `<< /Title (${title}) >>`,
  ];
  const parts: Buffer[] = [enc("%PDF-1.7\n")];
  const offsets: number[] = [];
  let pos = parts[0]!.length;
  objs.forEach((body, i) => {
    offsets.push(pos);
    const obj = enc(`${i + 1} 0 obj\n${body}\nendobj\n`);
    parts.push(obj);
    pos += obj.length;
  });
  let xref = `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    xref += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${objs.length + 1} /Root 1 0 R /Info 4 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
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

// every runTest() appends "<ms>\t<name>\t<pass|FAIL>" here, so a run can be
// picked apart afterwards ("which tests are slow, and did that change?").
// run-almost-all.ts / run-all.ts / run-pre-release.ts delete it before they start
export const TEST_TIMES_FILE = join(ROOT, ".work", "test-times.txt");

export function resetTestTimes(): void {
  mkdirSync(dirname(TEST_TIMES_FILE), { recursive: true });
  rmSync(TEST_TIMES_FILE, { force: true });
}

function recordTestTime(name: string, ms: number, ok: boolean): void {
  try {
    mkdirSync(dirname(TEST_TIMES_FILE), { recursive: true });
    appendFileSync(TEST_TIMES_FILE, `${ms.toFixed(0)}\t${name}\t${ok ? "pass" : "FAIL"}\n`);
  } catch {
    // timing is a nicety; never fail a test over it
  }
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
export async function runTest(name: string, fn: () => void | Promise<void>, opts?: RunTestOptions): Promise<void> {
  const silent = opts?.silent ?? false;
  const t0 = performance.now();
  const unmute = silent ? muteConsole() : () => {};
  try {
    await fn();
    unmute();
    recordTestTime(name, performance.now() - t0, true);
    const elapsed = formatDuration(performance.now() - t0);
    if (silent) {
      console.log(`== ${name} in ${elapsed}`);
    } else {
      console.log(`✅ ${name} passed in ${elapsed}`);
    }
  } catch (e) {
    unmute();
    recordTestTime(name, performance.now() - t0, false);
    const msg = (e as Error)?.message ?? e;
    throw new Error(`${name} failed after ${formatDuration(performance.now() - t0)}: ${msg}`);
  }
}

export function isSilentArg(argv: string[] = process.argv): boolean {
  return argv.includes("-silent") || argv.includes("--silent");
}

export type NamedTest = [string, () => void | Promise<void>];

export type SuiteOptions = {
  silent?: boolean;
  // run-pre-release.ts / run-all.ts reset the file themselves so suites share one log
  keepTestTimes?: boolean;
  heading?: string;
  summary?: boolean;
};

export async function runNamedTests(tests: NamedTest[], opts?: SuiteOptions): Promise<void> {
  if (!opts?.keepTestTimes) {
    resetTestTimes();
  }
  const silent = opts?.silent ?? false;
  const summary = opts?.summary ?? true;
  const t0 = performance.now();
  for (const [name, fn] of tests) {
    if (!silent) {
      console.log(`\n========== ${name} ==========`);
    }
    await runTest(name, fn, { silent });
  }
  if (summary && !silent) {
    const label = opts?.heading ?? "all";
    console.log(`\n✅ ${label}: ${tests.length} tests passed in ${formatDuration(performance.now() - t0)}`);
  }
}

export async function runSuiteMain(testit: (opts: SuiteOptions) => Promise<void>): Promise<void> {
  const silent = isSilentArg();
  if (!process.argv.includes("--no-build")) {
    buildApp({ silent });
  }
  try {
    await testit({ silent });
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}

// build SumatraPDF.exe the same way cmd/build.ts does
export function buildApp(opts?: { silent?: boolean }): void {
  if (!opts?.silent) {
    console.log("• building SumatraPDF.exe (cmd/build.ts) ...");
  }
  const p = Bun.spawnSync({
    cmd: ["bun", join(ROOT, "cmd", "build.ts"), "-debug"],
    cwd: ROOT,
    stdout: "inherit",
    stderr: "inherit",
  });
  if (p.exitCode !== 0) {
    throw new Error("build failed");
  }
}

// entry point for running a single test file directly:
//   bun tests/issue-<n>.ts [--no-build]
// builds (unless --no-build), runs testit(), exits 0 on pass / 1 on failure.
export async function runStandalone(testit: () => void | Promise<void>, name?: string): Promise<void> {
  const label = name ?? (process.argv[1] ?? "test").replace(/\\/g, "/").split("/").pop()!.replace(/\.ts$/, "");
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
