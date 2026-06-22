// Ad-hoc verification for https://github.com/sumatrapdfreader/sumatrapdf/issues/4567
// "Forward-search doesn't work when there're Chinese characters in the path."
//
// Builds a real LaTeX project inside a directory whose path contains Chinese
// characters, produces test.pdf + test.synctex, and runs the app's headless
// forward-search (SourceToDoc via the -dbg-control TestSynctex command). A
// success means SumatraPDF resolved the source line to a PDF rectangle despite
// the Chinese path.
//
// Notes on encoding (this machine's system ANSI code page is 1252 / Western):
//   - lualatex writes UTF-8 synctex -> robust for any script, incl. Chinese.
//   - pdflatex writes paths in the toolchain's encoding; we run it with a
//     relative input file so the synctex content is ASCII and only the *file
//     location* is Chinese (exercises CreateFileW-based reading). On a Chinese
//     system (ACP 936) pdflatex round-trips CJK via ANSI too.
//
// Run:  bun tests/ad-hoc-synctex-chinese.ts [--no-build]

import { gzipSync } from "node:zlib";
import { existsSync, mkdirSync, rmSync, readFileSync, writeFileSync, copyFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";

const DATA = join(import.meta.dir, "issue-5633-data");
const TEX_FILE = "test.tex";
const TARGET_LINE = 4; // line 4 of test.tex is a body paragraph synctex maps

// directory name with Chinese characters (UTF-8 on disk)
const CHINESE_DIR = "中文路径测试"; // "Chinese path test"

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

function run(cmd: string[], cwd: string) {
  const p = Bun.spawnSync({ cmd, cwd, stdout: "pipe", stderr: "pipe" });
  return { ok: p.exitCode === 0, stdout: p.stdout.toString(), stderr: p.stderr.toString() };
}

async function querySynctex(pdfPath: string, srcPath: string) {
  const [, rawArg] = await runControlCommand(EXE, ControlCommand.TestSynctex, [pdfPath, srcPath, TARGET_LINE]);
  const raw = String(rawArg).trim();
  const m = raw.match(/ret=(-?\d+)\s+page=(-?\d+)\s+nrects=(-?\d+)/);
  if (!m) {
    return { ret: -999, page: -1, nrects: -1, raw };
  }
  return { ret: parseInt(m[1]), page: parseInt(m[2]), nrects: parseInt(m[3]), raw };
}

async function runEngine(engine: "pdflatex" | "lualatex", enginePath: string): Promise<boolean> {
  // unique Chinese-named working dir under the gitignored tmp area
  const cwd = tmpPath(join("synctex-cn", `${CHINESE_DIR}-${engine}`));
  rmSync(cwd, { recursive: true, force: true });
  mkdirSync(cwd, { recursive: true });
  copyFileSync(join(DATA, TEX_FILE), join(cwd, TEX_FILE));

  console.log(`• ${engine} -synctex=-1 in ${cwd}`);
  const tex = run([enginePath, "-interaction=nonstopmode", "-synctex=-1", TEX_FILE], cwd);
  const pdfPath = join(cwd, "test.pdf");
  const synctexPath = join(cwd, "test.synctex");
  if (!existsSync(pdfPath) || !existsSync(synctexPath)) {
    console.error(tex.stdout.slice(-2000));
    console.error(tex.stderr.slice(-2000));
    throw new Error(`${engine} did not produce test.pdf + test.synctex in the Chinese-named dir`);
  }

  const plainBytes = readFileSync(synctexPath);
  const gzPath = join(cwd, "test.synctex.gz");
  const srcPath = join(cwd, TEX_FILE);

  const cases: { name: string; setup: () => void }[] = [
    {
      name: "plain .synctex",
      setup: () => {
        writeFileSync(synctexPath, plainBytes);
        rmSync(gzPath, { force: true });
      },
    },
    {
      name: "gzip .synctex.gz",
      setup: () => {
        rmSync(synctexPath, { force: true });
        writeFileSync(gzPath, gzipSync(plainBytes));
      },
    },
  ];

  let allPass = true;
  for (const c of cases) {
    c.setup();
    const res = await querySynctex(pdfPath, srcPath);
    const pass = res.ret === 0 && res.page >= 1 && res.nrects >= 1;
    allPass &&= pass;
    console.log(`  ${pass ? "PASS" : "FAIL"} ${engine} / ${c.name.padEnd(16)} -> ${res.raw.split("\n")[0]}`);
  }
  return allPass;
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }

  const engines: ("pdflatex" | "lualatex")[] = ["lualatex", "pdflatex"];
  const found: { engine: "pdflatex" | "lualatex"; path: string }[] = [];
  for (const e of engines) {
    const p = findLatexEngine(e);
    if (p) {
      found.push({ engine: e, path: p });
    } else {
      console.log(`• ${e} not found, skipping`);
    }
  }

  if (found.length === 0) {
    console.log(
      "\nSKIP: MiKTeX not installed, can't verify forward-search with Chinese paths.\n" +
        "Install with:  winget install MiKTeX.MiKTeX",
    );
    return;
  }

  let allPass = true;
  for (const { engine, path } of found) {
    allPass &&= await runEngine(engine, path);
  }
  if (!allPass) {
    throw new Error("forward-search failed for a Chinese-path case (issue #4567 not fixed)");
  }
  console.log("\nPASS: forward-search works with Chinese characters in the path (issue #4567)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
