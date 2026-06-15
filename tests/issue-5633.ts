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
import { basename, dirname, join } from "node:path";
import { EXE, runStandalone } from "./util.ts";

const DATA = join(import.meta.dir, "issue-5633-data");
const WORK = join(DATA, ".work");
const UNICODE_WORK = join(WORK, "synctex-zażółć");
const TEX_FILE = "test.tex";

// line 4 of issue-5633-data/test.tex is a body paragraph that synctex maps to a position
const TARGET_LINE = 4;

function findLatexEngine(engine: "pdflatex" | "lualatex"): string | null {
  const exe = `${engine}.exe`;
  const localAppData = process.env.LOCALAPPDATA ?? "";
  const candidates = [
    exe,
    join(localAppData, "Programs", "MiKTeX", "miktex", "bin", "x64", exe),
    join("C:/Program Files/MiKTeX/miktex/bin/x64", exe),
    join("C:/Program Files (x86)/MiKTeX/miktex/bin/x64", exe),
  ];
  for (const c of candidates) {
    if (c === exe) {
      const inPath = Bun.which(c);
      if (inPath) return inPath;
      continue;
    }
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
function querySynctex(
  pdfPath: string,
  srcPath: string,
  cwd: string,
): { ret: number; page: number; nrects: number; raw: string } {
  const outPath = join(cwd, "result.txt");
  rmSync(outPath, { force: true });
  const r = run([EXE, "-test-synctex", pdfPath, srcPath, String(TARGET_LINE), outPath], cwd);
  const raw = existsSync(outPath) ? readFileSync(outPath, "utf-8").trim() : `(no result file)\nstdout: ${r.stdout}\nstderr: ${r.stderr}`;
  const m = raw.match(/ret=(-?\d+)\s+page=(-?\d+)\s+nrects=(-?\d+)/);
  if (!m) {
    return { ret: -999, page: -1, nrects: -1, raw };
  }
  return { ret: parseInt(m[1]), page: parseInt(m[2]), nrects: parseInt(m[3]), raw };
}

type EngineCase = {
  engine: "pdflatex" | "lualatex";
  enginePath: string;
  cwd: string;
  srcPath: string;
};

function runEngineCase(engineCase: EngineCase): boolean {
  const srcName = basename(engineCase.srcPath);
  mkdirSync(engineCase.cwd, { recursive: true });
  copyFileSync(join(DATA, TEX_FILE), engineCase.srcPath);

  // produce a real test.pdf + uncompressed test.synctex (-synctex=-1 => no gzip)
  console.log(`• running ${engineCase.engine} -synctex=-1 in ${dirname(engineCase.srcPath)} ...`);
  const tex = run([engineCase.enginePath, "-interaction=nonstopmode", "-synctex=-1", srcName], engineCase.cwd);
  const pdfPath = join(engineCase.cwd, "test.pdf");
  const synctexPath = join(engineCase.cwd, "test.synctex");
  const gzPath = join(engineCase.cwd, "test.synctex.gz");
  if (!existsSync(pdfPath) || !existsSync(synctexPath)) {
    console.error(tex.stdout);
    console.error(tex.stderr);
    fail(`${engineCase.engine} did not produce test.pdf + test.synctex`);
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
    const res = querySynctex(pdfPath, engineCase.srcPath, engineCase.cwd);
    const pass = res.ret === 0 && res.page >= 1 && res.nrects >= 1;
    allPass &&= pass;
    const mark = pass ? "PASS" : "FAIL";
    console.log(`${mark} ${c.name.padEnd(26)} -> ${res.raw.split("\n")[0]}`);
    if (!pass && res.ret === -999) {
      console.log(`     ${res.raw}`);
    }
  }

  console.log("");
  return allPass;
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }

  // only run the engines that are actually installed; a missing engine (i.e.
  // MiKTeX not installed) is not a failure - we print a message and skip it
  const cases: EngineCase[] = [];
  const pdflatex = findLatexEngine("pdflatex");
  if (pdflatex) {
    console.log(`• pdflatex: ${pdflatex}`);
    cases.push({
      engine: "pdflatex",
      enginePath: pdflatex,
      cwd: join(WORK, "pdflatex"),
      srcPath: join(WORK, "pdflatex", TEX_FILE),
    });
  } else {
    console.log("• pdflatex not found, skipping");
  }
  const lualatex = findLatexEngine("lualatex");
  if (lualatex) {
    console.log(`• lualatex: ${lualatex}`);
    cases.push({
      engine: "lualatex",
      enginePath: lualatex,
      cwd: UNICODE_WORK,
      srcPath: join(UNICODE_WORK, TEX_FILE),
    });
  } else {
    console.log("• lualatex not found, skipping");
  }

  if (cases.length === 0) {
    console.log(
      "\nSKIP issue-5633: MiKTeX not installed, skipping synctex test.\n" +
        "To run it, install MiKTeX with:\n" +
        "    winget install MiKTeX.MiKTeX\n" +
        "pdflatex.exe / lualatex.exe are searched for in %PATH% and in:\n" +
        "    %LOCALAPPDATA%\\Programs\\MiKTeX\\miktex\\bin\\x64\\\n" +
        "    C:\\Program Files\\MiKTeX\\miktex\\bin\\x64\\",
    );
    return;
  }

  // fresh work dir
  rmSync(WORK, { recursive: true, force: true });

  let allPass = true;
  for (const c of cases) {
    console.log(`\n========== ${c.engine} ==========`);
    allPass &&= runEngineCase(c);
  }

  if (!allPass) {
    throw new Error("one or more synctex formats failed to resolve");
  }
  console.log("PASS all synctex formats resolved (issues #5633/#5678 fixed)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
