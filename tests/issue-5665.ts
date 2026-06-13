// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5665
//
// `SumatraPDF.exe run script.js` (the mupdf JavaScript `run` tool) must be able
// to read stdin via readline(). SumatraPDF.exe is a GUI-subsystem app, so its
// CRT stdin wasn't wired to the inherited input handle and readline() failed
// with "cannot read line from stdin". The fix wires stdin in
// fz_redirect_io_to_existing_console() (mupdf/source/tools/mudraw.c).
//
// This pipes two lines into `run issue-5665.js` and asserts the script read
// them back via readline().
//
// Run:  bun tests/issue-5665.ts [--no-build]   (or via tests/all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const SCRIPT = join(import.meta.dir, "issue-5665.js");

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  if (!existsSync(SCRIPT)) {
    throw new Error(`test script not found: ${SCRIPT}`);
  }

  // pipe two lines into the `run` tool (equivalent to `echo ... | SumatraPDF run script.js`)
  const input = "hello from stdin\nsecond line\n";
  const p = Bun.spawnSync({ cmd: [EXE, "run", SCRIPT], stdin: Buffer.from(input), stdout: "pipe", stderr: "pipe" });
  const out = p.stdout.toString();
  const err = p.stderr.toString();

  console.log("• `run` exit code:", p.exitCode);
  console.log("• captured stdout:");
  console.log(out.replace(/^/gm, "    "));
  if (err.trim()) {
    console.log("• captured stderr:");
    console.log(err.replace(/^/gm, "    "));
  }

  const problems: string[] = [];
  if (err.includes("cannot read line from stdin")) {
    problems.push('readline() failed: "cannot read line from stdin" (stdin not wired)');
  }
  if (!out.includes("got1:[hello from stdin]")) {
    problems.push('first readline() did not return "hello from stdin"');
  }
  if (!out.includes("got2:[second line]")) {
    problems.push('second readline() did not return "second line"');
  }

  if (problems.length === 0) {
    console.log("✅ `run` readline() read piped stdin (issue #5665 fixed)");
    return;
  }
  for (const pr of problems) {
    console.error(`  ❌ ${pr}`);
  }
  throw new Error("`run` readline() did not read stdin correctly");
}

if (import.meta.main) {
  await runStandalone(testit);
}
