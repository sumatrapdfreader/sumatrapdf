// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5681
//
// `SumatraPDF.exe run` interactive REPL used fgets(stdin) on a GUI app's
// console input, which spuriously returned EOF so the REPL exited and CMD
// interpreted the next lines as shell commands (`print` -> DOS PRINT, etc.).
// The fix uses ReadConsole for console stdin (fz_console_readline in mudraw.c).
//
// Piped multi-line REPL input exercises the same REPL loop without requiring
// interactive console automation.
//
// Run:  bun tests/issue-5681.ts [--no-build]   (or via tests/all.ts)

import { existsSync } from "node:fs";
import { EXE, runStandalone } from "./util.ts";

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }

  const input = "print(6*7)\nprint(6*8)\n";
  const p = Bun.spawnSync({
    cmd: [EXE, "run"],
    stdin: Buffer.from(input),
    stdout: "pipe",
    stderr: "pipe",
  });
  const out = p.stdout.toString();
  const err = p.stderr.toString();

  console.log("• `run` REPL exit code:", p.exitCode);
  console.log("• captured stdout:");
  console.log(out.replace(/^/gm, "    "));
  if (err.trim()) {
    console.log("• captured stderr:");
    console.log(err.replace(/^/gm, "    "));
  }

  const problems: string[] = [];
  if (p.exitCode !== 0) {
    problems.push(`exit code is ${p.exitCode}, expected 0`);
  }
  if (!out.includes("42")) {
    problems.push('first REPL expression did not print "42"');
  }
  if (!out.includes("48")) {
    problems.push('second REPL expression did not print "48"');
  }
  if (err.includes("cannot read line from stdin")) {
    problems.push('readline failed: "cannot read line from stdin"');
  }

  if (problems.length === 0) {
    console.log("✅ `run` REPL read multiple piped lines (issue #5681 regression)");
    return;
  }
  for (const pr of problems) {
    console.error(`  ❌ ${pr}`);
  }
  throw new Error("`run` REPL did not evaluate piped input correctly");
}

if (import.meta.main) {
  await runStandalone(testit);
}