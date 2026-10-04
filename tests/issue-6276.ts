// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6276
//
// `sumatrapdf-tool grep` printing UTF-8 matches to a console with a DBCS code
// page (936) failed with "cannot fwrite: No space left on device". The failure
// needs a real console, so the tool runs in a new one via `start /wait`.
//
// The access violation was a use-after-free: the rest of a matched line is
// printed at the next match, by which time the search had freed that page.
//
// Run:  bun tests/issue-6276.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util";

const FIXTURE = join(ROOT, "tests", "issue-6276.txt");
const DBCS_CODE_PAGE = 936;
const FILLER_LINES = 1000;

// the test copy of the exe has no sumatrapdf-tool.exe next to it
function findToolExe(): string | null {
  const candidates = [
    join(dirname(EXE), "sumatrapdf-tool.exe"),
    join(ROOT, "out", "dbg64", "sumatrapdf-tool.exe"),
    join(ROOT, "out", "rel64", "sumatrapdf-tool.exe"),
  ];
  return candidates.find((p) => existsSync(p)) ?? null;
}

// a match, many pages without one, then another match
function checkMatchesPagesApart(tool: string): void {
  const filler = "filler line\n".repeat(FILLER_LINES);
  const file = tmpPath("issue-6276-long.txt");
  writeFileSync(file, filler + "first 2023 rest of line\n" + filler + "second 2023\n");

  const r = Bun.spawnSync([tool, "grep", "2023", file]);
  const out = r.stdout.toString();
  if (r.exitCode !== 0 || !out.includes("first 2023 rest of line") || !out.includes("second 2023")) {
    throw new Error(`issue-6276: grep lost the end of a matched line: exit ${r.exitCode}, stdout: ${out}`);
  }
}

export async function testit(): Promise<void> {
  const tool = findToolExe();
  if (!tool) {
    console.log("  skipping: no sumatrapdf-tool.exe (build with `bun cmd/build.ts -dbg`)");
    return;
  }

  checkMatchesPagesApart(tool);

  const script = tmpPath("issue-6276.cmd");
  const exitFile = tmpPath("issue-6276-exit.txt");
  const errFile = tmpPath("issue-6276-err.txt");
  rmSync(exitFile, { force: true });
  rmSync(errFile, { force: true });

  const lines = [
    "@echo off",
    `chcp ${DBCS_CODE_PAGE} >nul`,
    `"${tool}" grep -n 2023 "${FIXTURE}" 2> "${errFile}"`,
    `echo %errorlevel% > "${exitFile}"`,
  ];
  writeFileSync(script, lines.join("\r\n") + "\r\n");

  Bun.spawnSync(["cmd.exe", "/c", "start", "/wait", "/min", "", "cmd.exe", "/c", script]);
  if (!existsSync(exitFile)) {
    console.log("  skipping: could not run the tool in a new console");
    return;
  }

  const exitCode = readFileSync(exitFile, "utf8").trim();
  const err = readFileSync(errFile, "utf8").trim();
  if (exitCode !== "0" || err !== "") {
    throw new Error(`issue-6276: grep to a cp${DBCS_CODE_PAGE} console failed: exit ${exitCode}, stderr: ${err}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
