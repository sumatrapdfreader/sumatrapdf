// `SumatraPDF cli markdown` (src/shared/Cli.cpp) on small generated PDFs, a
// case each: lists, running headers and page numbers, paragraphs split by a
// page or a picture, escaping, tables, footnote markers. Ported from the
// tests of Kitab's Markdown export (crates/kitab-mupdf/src/export.rs); the
// PDFs are made here.
//
// Run: bun tests/cli-markdown-cases.ts [--no-build]

import { writeFileSync } from "node:fs";
import { assemblePdf, EXE, makeZip, runStandalone, tmpPath } from "./util.ts";

const name = "cli-markdown-cases";

const FONTS =
  "/F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >> " +
  "/F2 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >> " +
  "/F3 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Oblique /Encoding /WinAnsiEncoding >> " +
  "/F4 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica-BoldOblique /Encoding /WinAnsiEncoding >>";

// `s` in `size` pt type (/F1 Helvetica, /F2 bold, /F3 italic) at x, y. A
// backslash in s is the PDF string's (\267 is a bullet, WinAnsi's middle dot);
// parentheses are text
function textAt(font: string, x: number, y: number, s: string, size = 10): string {
  const str = s.replace(/[()]/g, (m) => `\\${m}`);
  return `BT /${font} ${size} Tf ${x.toFixed(2)} ${y.toFixed(2)} Td (${str}) Tj ET\n`;
}

// one line of runs in different fonts, each [font, text], at x, y
function runsAt(x: number, y: number, runs: [string, string][], size = 10): string {
  let c = `BT ${x.toFixed(2)} ${y.toFixed(2)} Td `;
  for (const [font, s] of runs) {
    c += `/${font} ${size} Tf (${s.replace(/[()]/g, (m) => `\\${m}`)}) Tj `;
  }
  return c + "ET\n";
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
  // Windows text output ends lines with CRLF
  return p.stdout.toString().replace(/\r\n/g, "\n");
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

// inline_styles_keep_spaces_outside_markers
function testInlineStyles(): void {
  let c = runsAt(72, 700, [
    ["F1", "a "],
    ["F2", "bold "],
    ["F1", "and "],
    ["F3", "it"],
    ["F1", "."],
  ]);
  // in a sentence: a bold line on its own is a heading
  c += runsAt(72, 660, [
    ["F1", "this is "],
    ["F4", "both"],
    ["F1", "."],
  ]);
  // punctuation and spaces alone are never marked
  c += runsAt(72, 620, [
    ["F1", "x"],
    ["F2", ", "],
    ["F1", "y"],
  ]);
  const ls = lines(markdownOf([c]));
  for (const want of ["a **bold** and *it*.", "this is ***both***.", "x, y"]) {
    expect(`'${want}'`, ls.includes(want), ls.join("\n"));
  }
}

// escaping, pipes_and_ampersands_escaped_only_where_they_matter: markup
// characters in the text are escaped, # and > only at the start, & only
// when it starts an entity, | only at the start
function testEscaping(): void {
  const cases: [string, string][] = [
    ["2*3 = a_b [x] `c` <d> \\\\", "2\\*3 = a\\_b \\[x\\] \\`c\\` \\<d> \\\\"],
    ["# not a heading", "\\# not a heading"],
    ["> nope", "\\> nope"],
    ["2024. was a year", "2024\\. was a year"],
    ["plain", "plain"],
    ["*star* and #hash", "\\*star\\* and #hash"],
    ["| a | b", "\\| a | b"],
    ["a | b", "a | b"],
    ["&amp; & &#38; &#x26; &x AT&T", "\\&amp; & \\&#38; \\&#x26; &x AT&T"],
    ["Fish &amp; chips", "Fish \\&amp; chips"],
  ];
  // a PDF each: lines next to each other could be one paragraph
  for (const [text, want] of cases) {
    const md = markdownOf([textAt("F1", 72, 700, text)]);
    expect(`'${text}' as '${want}'`, lines(md).includes(want), md);
  }
  // a heading's closing # is text
  let h = textAt("F1", 72, 700, "C#", 24);
  for (let i = 0; i < 20; i++) {
    h += textAt("F1", 72, 650 - 14 * i, "Body text, long enough to be what most of the page is in.");
  }
  const hmd = markdownOf([h]);
  expect("heading 'C\\#'", lines(hmd).includes("# C\\#"), hmd);
}

// grid_becomes_a_table: a header in bold, prices flush right, an empty cell
// and a | in a cell
function testGridTable(): void {
  // Helvetica digits are 0.556 em and the point 0.278 em, so prices can be
  // set flush right at x = 300 like a real price column
  const right = (s: string) => 300 - [...s].reduce((w, ch) => w + (ch === "." ? 0.278 : 0.556), 0) * 10;
  let c = textAt("F1", 72, 740, "Fruit and what each one costs this week.");
  c += textAt("F2", 72, 700, "Name");
  c += textAt("F2", 180, 700, "Qty");
  c += textAt("F2", 300 - 24.46, 700, "Price");
  const rows = [
    ["Apple", "3", "1.50"],
    ["Kiwi|Gold", "12", "12.00"],
    ["Pear", "", "3.25"],
  ];
  rows.forEach(([nm, qty, price], i) => {
    const y = 680 - 20 * i;
    c += textAt("F1", 72, y, nm!);
    if (qty) {
      c += textAt("F1", 180, y, qty);
    }
    c += textAt("F1", right(price!), y, price!);
  });
  c += textAt("F1", 72, 600, "Prices are per kilo and change often.");
  const md = markdownOf([c]);
  const table =
    "| **Name** | **Qty** | **Price** |\n| --- | --- | --- |\n| Apple | 3 | 1.50 |\n" +
    "| Kiwi\\|Gold | 12 | 12.00 |\n| Pear | | 3.25 |";
  expect("the table", md.includes(table), md);
  expect("the paragraph before it", md.startsWith("Fruit and what each one costs this week.\n\n|"), md);
  expect("the paragraph after it", md.endsWith("|\n\nPrices are per kilo and change often.\n"), md);
}

// two_column_prose_is_not_a_table: two columns of prose, each line of the
// left one written just before the line beside it, so the rows line up
function testTwoColumnProse(): void {
  const left = [
    "the reader opens the book and finds that",
    "every page is set in two narrow columns",
    "which run down the sheet side by side and",
    "carry on from the foot of the left one to",
    "the head of the right one without a break.",
  ];
  const right = [
    "so a careless program that reads straight",
    "across the page would mix the two halves",
    "into nonsense and call the result a table",
    "of cells when it is really just running",
    "text that happens to share its baselines.",
  ];
  let c = "";
  left.forEach((l, i) => {
    c += textAt("F1", 72, 700 - 14 * i, l);
    c += textAt("F1", 330, 700 - 14 * i, right[i]!);
  });
  const md = markdownOf([c]);
  expect("no table", !lines(md).some((l) => l.startsWith("|")), md);
  expect("the text is all there", md.includes("finds that") && md.includes("baselines."), md);
}

// label_value_and_hanging_lists_are_not_tables
function testNotTables(): void {
  let c = textAt("F1", 72, 740, "Thank you for your order.");
  c += textAt("F1", 72, 720, "Total due:");
  c += textAt("F1", 400, 720, "42.00");
  c += textAt("F1", 72, 700, "Please pay within thirty days.");
  // bullets with a wide hanging indent line up like a two-column grid
  ["Bread", "Milk", "Eggs"].forEach((item, i) => {
    c += textAt("F1", 72, 660 - 14 * i, "\\267");
    c += textAt("F1", 100, 660 - 14 * i, item);
  });
  const md = markdownOf([c]);
  expect("no table", !lines(md).some((l) => l.startsWith("|")), md);
  expect("the text is all there", md.includes("Total due:") && md.includes("42.00") && md.includes("Eggs"), md);
}

// epub_table_becomes_a_table: an EPUB's <table>, its <th> row bold, not a heading
function testEpubTable(): void {
  const enc = new TextEncoder();
  const chapter =
    '<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title><style>td, th { padding: 0 1em; }</style></head>' +
    "<body><p>The formats and how <b>long</b> the samples are.</p><table>" +
    "<tr><th>Format</th><th>Pages</th></tr><tr><td>PDF</td><td>40</td></tr><tr><td>EPUB</td><td>5</td></tr>" +
    "</table><p>That is all of them.</p></body></html>";
  const opf =
    '<?xml version="1.0" encoding="utf-8"?>\n<package xmlns="http://www.idpf.org/2007/opf" version="3.0" ' +
    'unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">t</dc:identifier>' +
    "<dc:title>t</dc:title><dc:language>en</dc:language></metadata>" +
    '<manifest><item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/></manifest>' +
    '<spine><itemref idref="c1"/></spine></package>';
  const container =
    '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">' +
    '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>';
  const path = tmpPath("cli-md-case-table.epub");
  writeFileSync(
    path,
    makeZip([
      { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
      { name: "META-INF/container.xml", data: enc.encode(container) },
      { name: "OEBPS/c1.xhtml", data: enc.encode(chapter) },
      { name: "OEBPS/content.opf", data: enc.encode(opf) },
    ]),
  );
  const p = Bun.spawnSync([EXE, "cli", "markdown", path, "--no-page-markers"], { stdout: "pipe", stderr: "pipe" });
  const md = p.stdout.toString().replace(/\r\n/g, "\n");
  const table = "| **Format** | **Pages** |\n| --- | --- |\n| PDF | 40 |\n| EPUB | 5 |";
  expect("the EPUB's table", md.includes(table), md);
  expect("the header row isn't a heading", !md.includes("# Format"), md);
}

// heading_levels_cluster: the largest size is #, sizes within a point and a
// half are one level, all below the third level are ###
function testHeadingLevels(): void {
  let c = "";
  let y = 760;
  for (const size of [28, 48, 36, 47, 30, 60]) {
    y -= size + 12;
    c += textAt("F1", 72, y, `H${size}`, size);
  }
  const pages = [c];
  // the body: most of the text, in 10pt
  let body = "";
  for (let i = 0; i < 40; i++) {
    body += textAt("F1", 72, 740 - 14 * i, "Body text that is most of what the document is set in.");
  }
  pages.push(body);
  const ls = lines(markdownOf(pages));
  const want = ["# H60", "## H48", "## H47", "### H36", "### H30", "### H28"];
  for (const w of want) {
    expect(`'${w}'`, ls.includes(w), ls.join("\n").slice(0, 600));
  }
}

// not in Kitab: a footnote mark set smaller and raised is <sup>, a subscript
// set smaller and lowered <sub>, instead of digits glued to the word
function testSuperscripts(): void {
  const c =
    "BT /F1 10 Tf 72 700 Td (A claim) Tj /F1 6 Tf 4 Ts (1) Tj 0 Ts /F1 10 Tf ( and water is H) Tj " +
    "/F1 6 Tf -2 Ts (2) Tj 0 Ts /F1 10 Tf (O.) Tj ET\n";
  const md = markdownOf([c]);
  expect("<sup> and <sub>", lines(md).includes("A claim<sup>1</sup> and water is H<sub>2</sub>O."), md);
}

export async function testit(): Promise<void> {
  testLists();
  testPageNumbers();
  testRunningHeaders();
  testHyphenation();
  testParagraphAcrossPages();
  testInlineStyles();
  testEscaping();
  testGridTable();
  testTwoColumnProse();
  testNotTables();
  testEpubTable();
  testHeadingLevels();
  testSuperscripts();
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
