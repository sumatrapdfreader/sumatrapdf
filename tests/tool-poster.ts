// `SumatraPDF poster` (mupdf's pdfposter) splits each page into tiles. It used
// to fail on every file with "malformed page tree".
import { existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const SRC_PDF = join(ROOT, "tests", "issue-1189.pdf");

function run(args: string[]): { code: number; out: string } {
  const p = Bun.spawnSync([EXE, ...args]);
  return { code: p.exitCode ?? -1, out: p.stdout.toString() + p.stderr.toString() };
}

function pageCount(pdf: string): number {
  const r = run(["show", pdf, "pages"]);
  return r.out.split(/\r?\n/).filter((l) => /^page \d+ = /.test(l)).length;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("tool-poster");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const out = join(dir, "poster.pdf");

  const nSrc = pageCount(SRC_PDF);
  const r = run(["poster", "-x", "2", "-y", "2", SRC_PDF, out]);
  if (r.code !== 0 || !existsSync(out)) {
    throw new Error(`tool-poster: poster failed (exit ${r.code}):\n${r.out}`);
  }
  const n = pageCount(out);
  if (n !== nSrc * 4) {
    throw new Error(`tool-poster: ${nSrc} page(s) split 2x2 gave ${n} pages, want ${nSrc * 4}`);
  }
  console.log(`tool-poster: OK ${nSrc} -> ${n} pages`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
