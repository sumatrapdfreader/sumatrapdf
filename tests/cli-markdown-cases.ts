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

// page_numbers_and_romans: a page number alone in the bottom margin goes,
// words that only look like one stay
function testPageNumbers(): void {
  const yes = ["12", "xii", "XIV", "Page 3", "3 of 10", "- 4 -", "5 / 9", "page iv"];
  const no = ["mix", "Did", "Chapter 3", "In 1999 we", "Page"];
  for (const n of [...yes, ...no]) {
    const md = markdownOf([textAt("F1", 72, 400, "Some body text.", 11) + textAt("F1", 300, 30, n)]);
    const kept = lines(md).includes(n);
    expect(
      `'${n}' in the bottom margin ${yes.includes(n) ? "is" : "isn't"} a page number`,
      kept !== yes.includes(n),
      md,
    );
  }
}

// running_headers_and_page_numbers_are_dropped: a title in the top margin of
// every page and the page numbers go, the body stays
function testRunningHeaders(): void {
  const pages: string[] = [];
  for (let i = 0; i < 5; i++) {
    let c = textAt("F1", 72, 760, "The Book of Things");
    c += textAt("F1", 72, 400, `Body text on page ${i}.`, 11);
    c += textAt("F1", 300, 30, `${i + 1}`);
    pages.push(c);
  }
  const md = markdownOf(pages);
  expect("the running header goes", !md.includes("Book of Things"), md);
  expect("the body stays", md.includes("Body text on page 3."), md);
  expect("the page numbers go", !lines(md).some((l) => l.trim() === "3"), md);
  // a short document keeps its title
  const short = markdownOf(pages.slice(0, 2));
  expect("2 pages keep the title", short.includes("Book of Things"), short);
}

// hyphenation_is_joined: a word broken across lines is put back; a hyphen
// before a capital is a real one
function testHyphenation(): void {
  let c = textAt("F1", 72, 700, "an exam-");
  c += textAt("F1", 72, 688, "ple of well-");
  c += textAt("F1", 72, 676, "Known");
  const md = markdownOf([c]);
  expect("hyphenation", lines(md).includes("an example of well- Known"), md);
}

// running_headers_and_page_numbers_are_dropped, its second half: a paragraph
// a page break split is joined back (without page markers between)
function testParagraphAcrossPages(): void {
  const pages = [textAt("F1", 72, 100, "a sentence that goes"), textAt("F1", 72, 700, "on and on.")];
  let md = markdownOf(pages);
  expect("joined across the page break", md === "a sentence that goes on and on.\n", md);
  pages[1] = textAt("F1", 72, 700, "On a new sentence.");
  md = markdownOf(pages);
  expect("a capital starts a new paragraph", md.includes("goes\n\nOn a new"), md);
  md = markdownOf([textAt("F1", 72, 100, "an unfin-"), textAt("F1", 72, 700, "ished word")]);
  expect("a word broken across the page break", md.includes("an unfinished word"), md);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  testLists();
  testPageNumbers();
  testRunningHeaders();
  testHyphenation();
  testParagraphAcrossPages();
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
