// #5040: with a LaTeX editor that forward-searches after every compile
// (TeXnicCenter and friends), the view kept landing on the page the *previous*
// compile had put that line on -- for a document whose TOC pushed everything
// around, back at the beginning.
//
// Synchronizer::NeedsToRebuildIndex() stat'ed the .synctex path it was created
// with, but Create() always stores <base>.synctex even when the toolchain only
// wrote <base>.synctex.gz (what -synctex=1 produces, the MiKTeX/TeX Live
// default). That path never exists, so its timestamp was always 0, "has the
// sync file changed?" was never true, and the index built for the first compile
// was used for the whole session. Only reloading the PDF escaped it, because
// that builds a new Synchronizer -- which is why the default configuration
// mostly hid the bug.
//
// So the test forward-searches twice with ReloadModifiedDocuments = false: no
// reload, hence no new Synchronizer, and the second answer must still come from
// the second compile's synctex. Runs with both the gzipped and the plain
// synctex, since the plain one always worked.
import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath } from "./util";
import { sleep } from "./winapi";
import { findLatexEngine } from "./issue-5633";

const nSections = 8;
// how many pages the second compile drops in front of the sections
const nFiller = 5;

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
ReloadModifiedDocuments = false
EnableTeXEnhancements = true
`;

// a document like the report's: a table of contents, then one page per section.
// `filler` pages in front shift every section, so the same source line maps to a
// different page before and after the recompile.
function tex(filler: number): string {
  const pad = Array.from({ length: filler }, () => "\\clearpage\nFiller page.\n").join("");
  const sections = Array.from(
    { length: nSections },
    (_, i) => `\\clearpage\n\\section{Section ${i + 1}}\nBody of section ${i + 1}.\n`,
  ).join("");
  return `\\documentclass{article}
\\begin{document}
\\tableofcontents
${pad}${sections}\\end{document}
`;
}

// 1-based line of "\section{Section n}" in tex(filler)
function sectionLine(filler: number, n: number): number {
  const idx = tex(filler)
    .split("\n")
    .findIndex((l) => l === `\\section{Section ${n}}`);
  if (idx < 0) {
    throw new Error(`issue-5040: no line for section ${n}`);
  }
  return idx + 1;
}

async function compile(pdflatex: string, dir: string, filler: number, gzip: boolean): Promise<void> {
  writeFileSync(join(dir, "main.tex"), tex(filler));
  // -synctex=1 writes main.synctex.gz, -synctex=-1 an uncompressed main.synctex
  const p = Bun.spawnSync({
    cmd: [pdflatex, "-interaction=nonstopmode", `-synctex=${gzip ? 1 : -1}`, "-output-directory", dir, "main.tex"],
    cwd: dir,
    stdout: "ignore",
    stderr: "ignore",
  });
  if (!existsSync(join(dir, "main.pdf"))) {
    throw new Error(`issue-5040: pdflatex produced no main.pdf (exit ${p.exitCode})`);
  }
  const want = gzip ? "main.synctex.gz" : "main.synctex";
  if (!existsSync(join(dir, want))) {
    throw new Error(`issue-5040: pdflatex produced no ${want}`);
  }
}

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const m = /OK page=(\d+)/.exec(String(res[1] ?? ""));
  if (!m) {
    throw new Error(`issue-5040: can't read the current page: ${String(res[1] ?? "")}`);
  }
  return +m[1]!;
}

// what the editor does after a compile: tell the running instance to jump to the
// page holding a source line (the DDE ForwardSearch command, via the CLI)
async function forwardSearch(dir: string, line: number): Promise<void> {
  const p = Bun.spawn(
    [
      EXE,
      "-appdata",
      dir,
      "-reuse-instance",
      "-forward-search",
      join(dir, "main.tex"),
      String(line),
      join(dir, "main.pdf"),
    ],
    { stdout: "ignore", stderr: "ignore" },
  );
  await p.exited;
}

async function waitForPage(client: ControlClient, pred: (page: number) => boolean, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let last = 0;
  while (Date.now() < deadline) {
    last = await currentPage(client);
    if (pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`issue-5040: page never matched (last ${last})`);
}

async function checkSynctexFlavor(pdflatex: string, gzip: boolean): Promise<void> {
  const dir = tmpPath(`issue-5040-${gzip ? "gz" : "plain"}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  // first compile has the filler pages, so the sections sit late in the document
  await compile(pdflatex, dir, nFiller, gzip);

  const { first, second } = await withControlledSumatra(
    EXE,
    async (client) => {
      await forwardSearch(dir, sectionLine(nFiller, nSections));
      const first = await waitForPage(client, (p) => p > nFiller);

      // recompile without the filler: the same section is now nFiller pages
      // earlier. The PDF on screen stays the old one (no auto-reload), so the
      // Synchronizer -- and its index -- survives, which is the point.
      await compile(pdflatex, dir, 0, gzip);
      await forwardSearch(dir, sectionLine(0, nSections));
      const second = await waitForPage(client, (p) => p === first - nFiller);
      return { first, second };
    },
    ["-appdata", dir, join(dir, "main.pdf")],
  );

  const flavor = gzip ? "main.synctex.gz" : "main.synctex";
  if (first <= nFiller) {
    throw new Error(`issue-5040: ${flavor}: expected the first search to land past page ${nFiller}, got ${first}`);
  }
  if (second !== first - nFiller) {
    throw new Error(
      `issue-5040: ${flavor}: after the recompile the search landed on page ${second}, want ${first - nFiller} ` +
        `(the first compile put that section on ${first}; a stale synctex index answers ${first} or nearby)`,
    );
  }
  console.log(`issue-5040: ${flavor}: section ${nSections} on page ${first}, page ${second} after the recompile`);
}

export async function testit(): Promise<void> {
  const pdflatex = findLatexEngine("pdflatex");
  if (!pdflatex) {
    console.log(
      "SKIP issue-5040: MiKTeX not installed, skipping the synctex staleness test.\n" +
        "To run it, install MiKTeX with:\n    winget install MiKTeX.MiKTeX",
    );
    return;
  }
  console.log(`• pdflatex: ${pdflatex}`);
  await checkSynctexFlavor(pdflatex, true);
  await checkSynctexFlavor(pdflatex, false);
}

if (import.meta.main) {
  await runStandalone(testit);
}
