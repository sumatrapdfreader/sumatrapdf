// Run -dump-chm on all .chm files under the Sumatra test corpus and the repo.
// Writes tests/chm-dump-all-baseline.txt (or compares against it with --compare).
//
// Run:  bun tests/ad-hoc-chm-dump-all.ts
//       bun tests/ad-hoc-chm-dump-all.ts --compare

import { Glob } from "bun";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "fs";
import path from "path";
import { EXE, ROOT } from "./util.ts";

const BASELINE = path.join(ROOT, "tests/chm-dump-all-baseline.txt");
const OUT = path.join(ROOT, "tests/tmp/chm-dump-all-output.txt");

const roots = ["C:/Users/kjk/OneDrive/!sumatra", ROOT];

function collectChmFiles(): string[] {
  const seen = new Set<string>();
  const files: string[] = [];
  for (const root of roots) {
    if (!existsSync(root)) continue;
    const glob = new Glob("**/*.chm");
    for (const rel of glob.scanSync({ cwd: root, onlyFiles: true })) {
      const full = path.resolve(root, rel);
      const key = full.toLowerCase();
      if (!seen.has(key)) {
        seen.add(key);
        files.push(full);
      }
    }
  }
  files.sort();
  return files;
}

export async function testit(): Promise<void> {
  const compare = process.argv.includes("--compare");
  const files = collectChmFiles();
  if (files.length === 0) {
    throw new Error("no .chm files found");
  }

  mkdirSync(path.dirname(OUT), { recursive: true });

  let out = "";
  let exit0 = 0;
  let exitNonzero = 0;

  for (let i = 0; i < files.length; i++) {
    const f = files[i];
    process.stderr.write(`[${i + 1}/${files.length}] ${path.basename(f)}\n`);
    out += `=== ${f} ===\n`;
    const proc = Bun.spawn([EXE, "-for-testing", "-dump-chm", f], {
      stdout: "pipe",
      stderr: "pipe",
    });
    const [stdout, stderr, code] = await Promise.all([
      new Response(proc.stdout).text(),
      new Response(proc.stderr).text(),
      proc.exited,
    ]);
    out += stdout;
    if (stderr) out += stderr;
    out += `exit=${code}\n\n`;
    if (code === 0) exit0++;
    else exitNonzero++;
  }

  writeFileSync(OUT, out);
  console.log(`wrote ${OUT} (${out.length} bytes)`);
  console.log(`total=${files.length} exit0=${exit0} exit_nonzero=${exitNonzero}`);

  if (compare) {
    if (!existsSync(BASELINE)) {
      throw new Error(`missing baseline: ${BASELINE}`);
    }
    const a = readFileSync(BASELINE, "utf8");
    const b = readFileSync(OUT, "utf8");
    if (a !== b) {
      // DotZLib.Codec.html and CodecBaseMethods.html have run-to-run variable
      // decompressed bytes (pre-existing); ignore sha1 on those two lines only.
      const norm = (s: string) =>
        s.replace(
          /^(file class=.* path=\/DotZLib\.Codec(?:BaseMethods)?\.html)$/gm,
          (line) => line.replace(/sha1=[a-f0-9]{40}/, "sha1=*"),
        );
      if (norm(a) !== norm(b)) {
        throw new Error("MISMATCH: -dump-chm output differs from tests/chm-dump-all-baseline.txt");
      }
      console.log("MATCH: output identical to baseline (DotZLib sha1 ignored)");
    } else {
      console.log("MATCH: output identical to baseline");
    }
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}