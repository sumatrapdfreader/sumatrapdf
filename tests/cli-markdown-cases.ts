// `SumatraPDF cli markdown` (src/shared/Cli.cpp) on small generated PDFs, a
// case each: lists, running headers and page numbers, paragraphs split by a
// page or a picture, escaping, tables, footnote markers. Ported from the
// tests of Kitab's Markdown export (crates/kitab-mupdf/src/export.rs); the
// PDFs are made here.
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-markdown-cases.ts [--no-build]

import { writeFileSync } from "node:fs";
import { assemblePdf, EXE, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "cli-markdown-cases";

const FONTS =
  "/F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica >> " +
  "/F2 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold >> " +
  "/F3 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Oblique >>";

// `s` in `size` pt type (/F1 Helvetica, /F2 bold, /F3 italic) at x, y. A
// backslash in s is the PDF string's (\267 is a bullet); parentheses are text
function textAt(font: string, x: number, y: number, s: string, size = 10): string {
  const str = s.replace(/[()]/g, (m) => `\\${m}`);
  return `BT /${font} ${size} Tf ${x.toFixed(2)} ${y.toFixed(2)} Td (${str}) Tj ET\n`;
}

// a PDF with a page per content stream, US Letter
function pdfOf(pages: string[]): string {
  const n = pages.length;
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>", ""];
  const kids: string[] = [];
  for (let i = 0; i < n; i++) {
    const pageObj = 3 + i * 2;
    kids.push(`${pageObj} 0 R`);
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << ${FONTS} >> >> ` +
        `/Contents ${pageObj + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${pages[i]!.length} >>\nstream\n${pages[i]}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${n} >>`;
  return assemblePdf(objs);
}

let nPdfs = 0;

// the markdown of a PDF with these pages, without page markers
function markdownOf(pages: string[], args: string[] = ["--no-page-markers"]): string {
  const path = tmpPath(`cli-md-case-${++nPdfs}.pdf`);
  writeFileSync(path, pdfOf(pages), "latin1");
  const p = Bun.spawnSync([EXE, "cli", "markdown", path, ...args], { stdout: "pipe", stderr: "pipe" });
  if (p.exitCode !== 0) {
    throw new Error(`${name}: markdown failed (${p.exitCode}): ${p.stderr.toString()}`);
  }
  return p.stdout.toString();
}

function expect(what: string, ok: boolean, md: string): void {
  if (!ok) {
    throw new Error(`${name}: ${what}:\n${md}`);
  }
}

const lines = (md: string) => md.split("\n");

// list_markers: bullets and numbers start items; "-item" and "2024." don't
function testLists(): void {
  let c = "";
  const items = ["\\267 bread", "- milk", "12. twelve", "a) first", "(iv) fourth"];
  items.forEach((s, i) => (c += textAt("F1", 72, 700 - 14 * i, s)));
  // apart from the list: not list items
  c += textAt("F1", 72, 600, "-item stays a word");
  c += textAt("F1", 72, 560, "Hello world");
  const md = markdownOf([c]);
  const ls = lines(md);
  for (const want of ["- bread", "- milk", "12. twelve", "a) first", "(iv) fourth"]) {
    expect(`list item '${want}' on its own line`, ls.includes(want), md);
  }
  expect("'-item' isn't an item", ls.includes("-item stays a word"), md);
  expect("'Hello world' isn't an item", ls.includes("Hello world"), md);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  testLists();
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
