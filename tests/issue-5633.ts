// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5633
//
// Verifies that SumatraPDF's SyncTeX reverse/forward-search works for all three
// on-disk synctex layouts:
//   1. plain   .synctex           (uncompressed text)
//   2. gzip    .synctex.gz        (gzip with the .gz extension)
//   3. gzip-in-.synctex           (gzip bytes stored in a plain-named .synctex)
//
// It builds the app (same as cmd/build.ts), uses MiKTeX's pdflatex to produce a
// real PDF + .synctex pair, derives the three layouts from it, and runs the
// headless `-test-synctex` flag (added for this test) which performs a
// SourceToDoc() forward-search query and writes the result to a file.
//
// Run:  bun tests/issue-5633.ts [--no-build]   (or via tests/all.ts)
//
// FFI / Windows APIs are not needed here (the app reports results via a result
// file), so there is no tests/winapi.ts. If a future test needs to read GUI
// state, that's where the FFI wrappers would go.

import { gzipSync } from "node:zlib";
import { existsSync, mkdirSync, rmSync, readFileSync, writeFileSync, copyFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const DATA = join(import.meta.dir, "issue-5633-data");
const WORK = join(DATA, ".work");

// line 4 of issue-5633-data/test.tex is a body paragraph that synctex maps to a position
const TARGET_LINE = 4;

function findPdflatex(): string | null {
  const inPath = Bun.which("pdflatex");
  if (inPath) return inPath;
  const localAppData = process.env.LOCALAPPDATA ?? "";
  const candidates = [
    join(localAppData, "Programs", "MiKTeX", "miktex", "bin", "x64", "pdflatex.exe"),
    "C:/Program Files/MiKTeX/miktex/bin/x64/pdflatex.exe",
    "C:/Program Files (x86)/MiKTeX/miktex/bin/x64/pdflatex.exe",
  ];
  for (const c of candidates) {
    if (c && existsSync(c)) return c;
  }
  return null;
}

function fail(msg: string): never {
  throw new Error(msg);
}

function run(cmd: string[], cwd: string): { ok: boolean; stdout: string; stderr: string } {
  const p = Bun.spawnSync({ cmd, cwd, stdout: "pipe", stderr: "pipe" });
  return {
    ok: p.exitCode === 0,
    stdout: p.stdout.toString(),
    stderr: p.stderr.toString(),
  };
}

// runs the app's headless synctex forward-search for the given pdf and returns
// the parsed result (ret is a PDFSYNCERR_* code; 0 == PDFSYNCERR_SUCCESS)
function querySynctex(pdfPath: string): { ret: number; page: number; nrects: number; raw: string } {
  const outPath = join(WORK, "result.txt");
  rmSync(outPath, { force: true });
  const r = run([EXE, "-test-synctex", pdfPath, "test.tex", String(TARGET_LINE), outPath], WORK);
  const raw = existsSync(outPath) ? readFileSync(outPath, "utf-8").trim() : `(no result file)\nstdout: ${r.stdout}\nstderr: ${r.stderr}`;
  const m = raw.match(/ret=(-?\d+)\s+page=(-?\d+)\s+nrects=(-?\d+)/);
  if (!m) {
    return { ret: -999, page: -1, nrects: -1, raw };
  }
  return { ret: parseInt(m[1]), page: parseInt(m[2]), nrects: parseInt(m[3]), raw };
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }

  const pdflatex = findPdflatex();
  if (!pdflatex) {
    fail(
      "MiKTeX (pdflatex) not found.\n\n" +
        "Install it with:\n" +
        "    winget install MiKTeX.MiKTeX\n\n" +
        "pdflatex is searched for in %PATH% and in:\n" +
        "    %LOCALAPPDATA%\\Programs\\MiKTeX\\miktex\\bin\\x64\\pdflatex.exe\n" +
        "    C:\\Program Files\\MiKTeX\\miktex\\bin\\x64\\pdflatex.exe",
    );
  }
  console.log(`• pdflatex: ${pdflatex}`);

  // fresh work dir
  rmSync(WORK, { recursive: true, force: true });
  mkdirSync(WORK, { recursive: true });
  copyFileSync(join(DATA, "test.tex"), join(WORK, "test.tex"));

  // produce a real test.pdf + uncompressed test.synctex (-synctex=-1 => no gzip)
  console.log("• running pdflatex -synctex=-1 ...");
  const tex = run([pdflatex, "-interaction=nonstopmode", "-synctex=-1", "test.tex"], WORK);
  const pdfPath = join(WORK, "test.pdf");
  const synctexPath = join(WORK, "test.synctex");
  const gzPath = join(WORK, "test.synctex.gz");
  if (!existsSync(pdfPath) || !existsSync(synctexPath)) {
    console.error(tex.stdout);
    console.error(tex.stderr);
    fail("pdflatex did not produce test.pdf + test.synctex");
  }

  const plainBytes = readFileSync(synctexPath);
  const gzBytes = gzipSync(plainBytes);
  // sanity: gzip magic
  if (!(gzBytes[0] === 0x1f && gzBytes[1] === 0x8b)) {
    fail("internal: gzip output lacks magic bytes");
  }

  type Case = { name: string; setup: () => void };
  const cases: Case[] = [
    {
      name: "1. plain .synctex",
      setup: () => {
        writeFileSync(synctexPath, plainBytes);
        rmSync(gzPath, { force: true });
      },
    },
    {
      name: "2. gzip .synctex.gz",
      setup: () => {
        rmSync(synctexPath, { force: true });
        writeFileSync(gzPath, gzBytes);
      },
    },
    {
      name: "3. gzip bytes in .synctex",
      setup: () => {
        rmSync(gzPath, { force: true });
        writeFileSync(synctexPath, gzBytes);
      },
    },
  ];

  console.log("");
  let allPass = true;
  for (const c of cases) {
    c.setup();
    const res = querySynctex(pdfPath);
    const pass = res.ret === 0 && res.page >= 1 && res.nrects >= 1;
    allPass &&= pass;
    const mark = pass ? "✅" : "❌";
    console.log(`${mark} ${c.name.padEnd(26)} -> ${res.raw.split("\n")[0]}`);
    if (!pass && res.ret === -999) {
      console.log(`     ${res.raw}`);
    }
  }

  console.log("");
  if (!allPass) {
    throw new Error("one or more synctex formats failed to resolve");
  }
  console.log("✅ all 3 synctex formats resolved (issue #5633 fixed)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
