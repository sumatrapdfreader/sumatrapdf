// Shared helpers for tests/. See agents.md "Writing tests".
//
// Each tests/issue-<n>.ts exports `async function testit()` that runs the test
// and THROWS on failure (returns normally on success). It does NOT build the app
// or call process.exit -- that's the runner's job, so tests compose in
// run-almost-all.ts / run-all.ts.

import {
  appendFileSync,
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, resolve } from "node:path";
import { enumWindows, getWindowPid, getWindowText, hasInteractiveDesktop } from "./winapi.ts";

export const ROOT = join(import.meta.dir, "..");

// `-exe <path>` runs the tests against an executable that is already built,
// e.g. a release or an ASan one, or a build from another checkout. Validated
// here so a typo fails immediately instead of every test timing out.
function exeFromArgv(argv: string[]): string {
  const i = argv.indexOf("-exe");
  if (i < 0) {
    return "";
  }
  const bail = (why: string): never => {
    console.error(`-exe: ${why}`);
    process.exit(1);
  };
  const path = argv[i + 1];
  if (!path || path.startsWith("-")) {
    bail("expected a path to SumatraPDF.exe");
  }
  const full = resolve(path!);
  if (!existsSync(full) || !statSync(full).isFile()) {
    bail(`no such file: ${full}`);
  }
  if (!/\.exe$/i.test(full)) {
    bail(`not an executable: ${full}`);
  }
  // the name can be anything (SumatraPDF-static.exe, a renamed copy), so ask
  // the file itself what it is
  const ps = `(Get-Item -LiteralPath '${full}').VersionInfo.ProductName`;
  const p = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  const product = p.stdout.toString().trim();
  if (!/sumatrapdf/i.test(product)) {
    bail(`not a SumatraPDF executable (ProductName is '${product}'): ${full}`);
  }
  return full;
}

// The exe under test: the debug build, unless `-exe <path>` or SUMATRA_TEST_EXE
// names another one. The GitHub CI job sets the environment variable to the
// debug ASan build (out/dbg64_asan/SumatraPDF-static.exe), which is the same app
// plus ASan. Both are read at import time, before any test runs.
export const EXE_FROM_ARGV = exeFromArgv(process.argv);
const SOURCE_EXE = EXE_FROM_ARGV || process.env.SUMATRA_TEST_EXE || join(ROOT, "out", "dbg64", "SumatraPDF.exe");

// Keep the executable and its portable settings file in a fresh directory for
// every test run. This prevents a manual run, or an earlier test that saves
// settings, from changing the starting state of later tests.
export const TESTS_TMP_DIR = join(ROOT, ".work", "tests-tmp");
export let EXE = SOURCE_EXE;

export function prepareTestEnvironment(): void {
  const sourceExe = resolve(SOURCE_EXE);
  const exeName = sourceExe.split("\\").pop()!;
  const sourcePdb = sourceExe.replace(/\.exe$/i, ".pdb");
  if (!existsSync(sourcePdb)) {
    throw new Error(`test executable PDB not found: ${sourcePdb}`);
  }
  rmSync(TESTS_TMP_DIR, { recursive: true, force: true });
  mkdirSync(TESTS_TMP_DIR, { recursive: true });
  const testExe = join(TESTS_TMP_DIR, exeName);
  copyFileSync(sourceExe, testExe);
  copyFileSync(sourcePdb, join(TESTS_TMP_DIR, sourcePdb.split("\\").pop()!));
  EXE = testExe;
}

// An ASan build renders and starts several times slower than the debug one, so
// waits sized for a debug build time out against it (a 25600% zoom needs far
// more than the 30s issue-1195 asks for). Tests don't size their own waits for
// it: control.ts multiplies its timeouts by this.
export const SLOW_BUILD_FACTOR = /asan/i.test(EXE) ? 4 : 1;

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

// Assemble a PDF from object bodies (no "n 0 obj" wrappers). Object 1 must be
// the catalog. Offsets use latin1 byte length so a binary header stays valid.
export function assemblePdf(objects: string[], opts?: { header?: string; trailerExtras?: string }): string {
  let body = opts?.header ?? "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objects.length; i++) {
    offsets.push(Buffer.byteLength(body, "latin1"));
    body += `${i + 1} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xrefStart = Buffer.byteLength(body, "latin1");
  const size = objects.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  const extras = opts?.trailerExtras ? ` ${opts.trailerExtras}` : "";
  body += `trailer\n<< /Size ${size} /Root 1 0 R${extras} >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

// One-page US-letter PDF. `annots` are /Annot dictionaries on the page.
export function makeOnePagePdf(opts?: { annots?: string[]; mediaBox?: string; header?: string }): string {
  const annots = opts?.annots ?? [];
  const media = opts?.mediaBox ?? "[0 0 612 792]";
  const annotsEntry = annots.length > 0 ? ` /Annots [${annots.join(" ")}]` : "";
  return assemblePdf(
    [
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      `<< /Type /Page /Parent 2 0 R /MediaBox ${media}${annotsEntry} >>`,
    ],
    { header: opts?.header },
  );
}

// A valid one-page PDF with no content, for tests that only need a document the
// app can open (a link target, a tab to switch to, ...). The xref offsets are
// computed, so mupdf loads it without running its repair pass. For a PDF with
// actual text on the page see makeTextPdf in tests/issue-5922.ts.
export function makeMinimalPdf(title: string): Buffer {
  return Buffer.from(
    assemblePdf(
      [
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
        `<< /Title (${title}) >>`,
      ],
      { header: "%PDF-1.7\n", trailerExtras: "/Info 4 0 R" },
    ),
    "latin1",
  );
}

// Fresh -appdata directory with SumatraPDF-settings.txt. Tests that need extra
// files write them into the returned path.
export function writeAppdata(name: string, settings: string): string {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const body = settings.endsWith("\n") ? settings : `${settings}\n`;
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), body);
  return dir;
}

export async function pollUntil<T>(
  fn: () => T | Promise<T>,
  ok: (value: T) => boolean,
  opts?: { timeoutMs?: number; intervalMs?: number; error?: string | ((last: T) => string) },
): Promise<T> {
  const timeoutMs = opts?.timeoutMs ?? 8000;
  const intervalMs = opts?.intervalMs ?? 50;
  const deadline = Date.now() + timeoutMs;
  let last!: T;
  for (;;) {
    last = await fn();
    if (ok(last)) {
      return last;
    }
    if (Date.now() >= deadline) {
      const err = opts?.error;
      throw new Error(typeof err === "function" ? err(last) : (err ?? "timed out"));
    }
    await Bun.sleep(intervalMs);
  }
}

export function findAnnotWindow(pid: number, frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (hwnd !== frame && getWindowPid(hwnd) === pid && getWindowText(hwnd).startsWith("Annotations")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

export async function waitForAnnotWindow(pid: number, frame: number, timeoutMs = 10_000): Promise<number> {
  return pollUntil(
    () => findAnnotWindow(pid, frame),
    (hwnd) => hwnd !== 0,
    {
      timeoutMs,
      intervalMs: 50,
      error: "Annotations window did not open",
    },
  );
}

// TestDpi (`ControlCommand.TestDpi`) helpers. The command id is imported lazily
// so util.ts does not cycle with control.ts at load time.
export async function dpiRequest(
  client: { request(command: number, args?: unknown[]): Promise<unknown[]> },
  action: string,
): Promise<string> {
  const { ControlCommand } = await import("./control.ts");
  const response = await client.request(ControlCommand.TestDpi, [action]);
  const code = Number(response[0] ?? -1);
  const raw = String(response[1] ?? "");
  if (code !== 0) {
    throw new Error(`TestDpi ${action} failed (${code}): ${raw.trim()}`);
  }
  return raw;
}

export function parseDpiFields(raw: string): Record<string, number> {
  const values: Record<string, number> = {};
  for (const match of raw.matchAll(/(\w+)=(\d+)/g)) {
    values[match[1]] = Number(match[2]);
  }
  return values;
}

export async function waitForDpiFrame(
  client: { request(command: number, args?: unknown[]): Promise<unknown[]> },
  expectedDpi: number,
  ready: (fields: Record<string, number>, raw: string) => boolean,
  timeoutMs = 5000,
): Promise<{ fields: Record<string, number>; raw: string }> {
  const load = async () => {
    const raw = await dpiRequest(client, "state");
    return { fields: parseDpiFields(raw), raw };
  };
  return pollUntil(load, (s) => s.fields.frame === expectedDpi && ready(s.fields, s.raw), {
    timeoutMs,
    intervalMs: 40,
    error: (s) => `DPI state did not settle at ${expectedDpi}: ${s.raw.trim()}`,
  });
}

export function requireDpiShrank(name: string, high: number, low: number): void {
  if (high <= 0 || low <= 0 || high < low * 1.3) {
    throw new Error(`${name} did not follow 150% -> 75% DPI (${high} -> ${low})`);
  }
}

// Directory for temporary / scratch files produced by tests. It lives below
// TESTS_TMP_DIR so explicit -appdata directories are isolated from the user's
// settings and from the executable used by the test. Use tmpPath() to get a
// path inside it (dir created on demand).
export const TMP_DIR = join(TESTS_TMP_DIR, "tmp");

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

// "3/34 issue-5964.ts" before each test: what is about to run, and how far along
// the suite is. The outermost suite sets the total; a suite nested in another
// one (run-almost-all inside run-all / run-pre-release) leaves it alone.
let progressTotal = 0;
let progressDone = 0;

export function startSuiteProgress(total: number): void {
  if (progressTotal > 0) {
    return;
  }
  progressTotal = total;
  progressDone = 0;
}

// run one test and print pass/fail timing
export async function runTest(name: string, fn: () => void | Promise<void>, opts?: RunTestOptions): Promise<void> {
  const silent = opts?.silent ?? false;
  const progress = progressTotal > 0 ? `${++progressDone}/${progressTotal} ${name}.ts` : "";
  // A silent run prints nothing while a test runs, so the line stays open and
  // the timing completes it: "3/34 issue-5964.ts in 2.0s". A verbose run has
  // the test's own output coming next, so end the line right away.
  let lineOpen = false;
  if (progress) {
    if (silent) {
      process.stdout.write(progress);
      lineOpen = true;
    } else {
      console.log(progress);
    }
  }
  const t0 = performance.now();
  const unmute = silent ? muteConsole() : () => {};
  try {
    await fn();
    unmute();
    recordTestTime(name, performance.now() - t0, true);
    const elapsed = formatDuration(performance.now() - t0);
    if (lineOpen) {
      console.log(` in ${elapsed}`);
    } else if (silent) {
      console.log(`== ${name} in ${elapsed}`);
    } else {
      console.log(`✅ ${name} passed in ${elapsed}`);
    }
  } catch (e) {
    unmute();
    if (lineOpen) {
      // don't glue the caller's error message onto the open line
      console.log("");
    }
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
  if (summary) {
    const label = opts?.heading ?? "all";
    console.log(`\n✅ ${label}: ${tests.length} tests passed in ${formatDuration(performance.now() - t0)}`);
  }
}

// A locked screen or a disconnected RDP session has no interactive desktop, so
// every test that hovers or drives the real cursor fails for reasons that have
// nothing to do with the code. Say so up front instead of letting it look
// like a regression.
function checkInteractiveDesktop(): void {
  if (hasInteractiveDesktop() || process.argv.includes("-allow-locked-desktop")) {
    return;
  }
  const msg = [
    "",
    "❌ no interactive desktop: this session is locked or disconnected.",
    "   Tests that move the real cursor cannot pass here.",
    "   Unlock the console, or hand an RDP session back to it before disconnecting:",
    "     query session && tscon <id> /dest:console",
    "   Pass -allow-locked-desktop to run the suite anyway.",
  ].join("\n");
  console.error(msg);
  process.exit(1);
}

export async function runSuiteMain(testit: (opts: SuiteOptions) => Promise<void>): Promise<void> {
  const silent = isSilentArg();
  checkInteractiveDesktop();
  // an exe named with -exe is the one to test, so don't build over it
  if (!process.argv.includes("--no-build") && !EXE_FROM_ARGV) {
    buildApp({ silent });
  }
  prepareTestEnvironment();
  const t0 = performance.now();
  try {
    await testit({ silent });
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e} (after ${formatDuration(performance.now() - t0)})`);
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
  // a single test is worth running locked (most don't touch the cursor), but
  // say so, or a cursor-driven failure reads as a bug in the code
  if (!hasInteractiveDesktop()) {
    console.error("⚠ this session is locked or disconnected: tests that move the real cursor will fail");
  }
  try {
    if (!process.argv.includes("--no-build") && !EXE_FROM_ARGV) {
      buildApp();
    }
    prepareTestEnvironment();
    await runTest(label, testit);
  } catch (e) {
    console.error(`\n❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
  process.exit(0);
}
