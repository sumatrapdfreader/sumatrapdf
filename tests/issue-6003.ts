// Installer -x extracts the payload without installing (issue #6003).
// Copy the exe into a temp dir as SumatraPDF.exe, then check:
//   -x -d out  writes SumatraPDF.exe plus the payload under out/
//   -x         unpacks the payload next to the running exe and does not
//              overwrite that exe
//
// Run: bun tests/issue-6003.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync, rmSync, statSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const PAYLOAD = ["libsumatrapdf.dll", "PdfFilter.dll", "PdfPreview.dll", "sumatrapdf-tool.exe"];

function runExtract(copied: string, cwd: string, args: string[]): { out: string; exitCode: number } {
  const p = Bun.spawnSync({
    cmd: [copied, ...args],
    cwd,
    stdout: "pipe",
    stderr: "pipe",
    timeout: 60_000,
  });
  return {
    out: (p.stdout.toString() + p.stderr.toString()).trim(),
    exitCode: p.exitCode ?? 1,
  };
}

function mustExist(dir: string, names: string[]): void {
  for (const name of names) {
    const path = join(dir, name);
    if (!existsSync(path)) {
      throw new Error(`issue-6003: missing '${path}'`);
    }
    if (statSync(path).size <= 0) {
      throw new Error(`issue-6003: empty '${path}'`);
    }
  }
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`issue-6003: app not found: ${EXE}`);
  }
  if (/static/i.test(EXE)) {
    console.log("issue-6003: skip, static exe has no installer payload (-x is installer-only)");
    return;
  }

  const dir = tmpPath("issue-6003");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const copied = join(dir, "SumatraPDF.exe");
  copyFileSync(EXE, copied);
  const outDir = join(dir, "out");
  mkdirSync(outDir);

  const toOut = runExtract(copied, dir, ["-x", "-d", "out"]);
  if (/not a SumatraPDF installer/i.test(toOut.out)) {
    console.log("issue-6003: skip, this exe has no installer payload");
    return;
  }
  if (toOut.exitCode !== 0) {
    throw new Error(`issue-6003: -x -d out exited ${toOut.exitCode}:\n${toOut.out}`);
  }
  mustExist(outDir, ["SumatraPDF.exe", ...PAYLOAD]);

  const before = statSync(copied);
  const inPlace = runExtract(copied, dir, ["-x"]);
  if (inPlace.exitCode !== 0) {
    throw new Error(`issue-6003: -x exited ${inPlace.exitCode}:\n${inPlace.out}`);
  }
  mustExist(dir, PAYLOAD);
  if (!existsSync(copied)) {
    throw new Error("issue-6003: in-place -x removed SumatraPDF.exe");
  }
  const after = statSync(copied);
  if (after.size !== before.size || after.mtimeMs !== before.mtimeMs) {
    throw new Error("issue-6003: in-place -x overwrote SumatraPDF.exe");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
