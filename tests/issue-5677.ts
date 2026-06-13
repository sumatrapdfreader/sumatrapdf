// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5677
//
// `SumatraPDF.exe info file.pdf` (and the other mupdf-derived tools) must write
// ONLY the tool's output to stdout, so that `info file.pdf > out.txt` captures
// clean output. Before the fix:
//   - the tool wasn't even detected when the exe was launched via a relative
//     path (the GUI launched instead),
//   - SumatraPDF's internal logging leaked onto stdout, and
//   - fz_redirect_io_to_existing_console() re-bound stdout to the console,
//     discarding `> file` / pipe redirection.
//
// This runs `info issue-5677.pdf`, captures stdout, and asserts it contains the
// expected PDF info and none of the internal log noise.
//
// Run:  bun tests/issue-5677.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";

const SCRIPT_DIR = import.meta.dir;
const ROOT = join(SCRIPT_DIR, "..");
const EXE = join(ROOT, "out", "dbg64", "SumatraPDF-dll.exe");
const PDF = join(SCRIPT_DIR, "issue-5677.pdf");

// must appear in `info` output for our test pdf (proves the tool ran and read it)
const INFO_MARKERS = ["PDF-1.7", "Pages: 1", "Issue 5677 Test"];
// internal log lines that must NOT contaminate the tool's stdout
const LOG_MARKERS = [
  "ver 3.7",
  "OpenEmbeddedFilesArchive",
  "ParseFlags",
  "LoadSettings",
  "ReadRegStr",
  "uitask::",
  "GetExistingInstallationDir",
];

function fail(msg: string): never {
  console.error(`\n❌ ${msg}`);
  process.exit(1);
}

function buildApp() {
  console.log("• building SumatraPDF-dll.exe (cmd/build.ts) ...");
  const p = Bun.spawnSync({ cmd: ["bun", join(ROOT, "cmd", "build.ts")], cwd: ROOT, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    fail("build failed");
  }
}

function main() {
  const noBuild = process.argv.includes("--no-build");
  if (!noBuild) {
    buildApp();
  }
  if (!existsSync(EXE)) {
    fail(`app not found: ${EXE} (run without --no-build)`);
  }
  if (!existsSync(PDF)) {
    fail(`test pdf not found: ${PDF}`);
  }

  // capture stdout via a pipe (equivalent to `info file.pdf > out.txt`)
  const p = Bun.spawnSync({ cmd: [EXE, "info", PDF], stdout: "pipe", stderr: "pipe" });
  const out = p.stdout.toString();

  console.log("• `info` exit code:", p.exitCode);
  console.log("• captured stdout:");
  console.log(out.replace(/^/gm, "    "));

  const problems: string[] = [];
  if (p.exitCode !== 0) {
    problems.push(`exit code is ${p.exitCode}, expected 0`);
  }
  for (const m of INFO_MARKERS) {
    if (!out.includes(m)) {
      problems.push(`missing expected info marker: "${m}"`);
    }
  }
  for (const m of LOG_MARKERS) {
    if (out.includes(m)) {
      problems.push(`stdout contaminated with log line: "${m}"`);
    }
  }

  if (problems.length === 0) {
    console.log("\n✅ `info` produced clean output (issue #5677 fixed)");
    process.exit(0);
  }
  for (const p of problems) {
    console.error(`  ❌ ${p}`);
  }
  fail("`info` output is not clean");
}

main();
