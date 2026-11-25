// `SumatraPDF cli markdown` and `cli text` (src/shared/Cli.cpp) on whole
// documents: a 40-page PDF with an outline, an EPUB, pictures, an encrypted
// PDF, FB2, XPS and right-to-left text. Ported from Kitab's export tests
// (crates/kitab-mupdf/src/export.rs: text_outline_headings_and_paragraphs,
// page_selection_markers_and_range, pictures_saved_and_linked_or_left_out,
// epub_converts_with_needles, plain_text_has_needles,
// password_and_other_formats); the documents are made here.
//
// Run: bun tests/cli-markdown-docs.ts [--no-build]

import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, EXE, makeZip, ROOT, runStandalone, tmpPath } from "./util.ts";

const name = "cli-markdown-docs";

type Res = { code: number; out: string; err: string };

function cli(args: string[]): Res {
  const p = Bun.spawnSync([EXE, "cli", ...args], { stdout: "pipe", stderr: "pipe" });
  // Windows text output ends lines with CRLF
  return { code: p.exitCode ?? -1, out: p.stdout.toString().replace(/\r\n/g, "\n"), err: p.stderr.toString() };
}

function expect(what: string, ok: boolean, res: Res | string): void {
  if (!ok) {
    const s = typeof res === "string" ? res : `exit ${res.code}\nstdout: ${res.out}\nstderr: ${res.err}`;
    throw new Error(`${name}: ${what}:\n${s.slice(0, 3000)}`);
  }
}

const needle = (n: number) => `sumatra-needle-${String(n).padStart(2, "0")}`;

// 40 pages; a chapter (18pt title, in the outline) starts every 4th page;
// page n's body has needle(n)
function makeOutlinedPdf(): string {
  const nPages = 40;
  const font = "/F1 << /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const objs: string[] = ["<< /Type /Catalog /Pages 2 0 R /Outlines 3 0 R >>", "", ""];
  const pageObj = (i: number) => 4 + i * 2;
  const chapters: number[] = [];
  for (let i = 0; i < nPages; i++) {
    let c = "";
    if (i % 4 === 0) {
      chapters.push(i);
      c += `BT /F1 18 Tf 72 720 Td (Chapter ${i / 4 + 1}) Tj ET\n`;
    }
    const body = [
      `This is the body of page ${i + 1}, which holds ${needle(i + 1)} in its first line`,
      "and goes on for a few more lines of ordinary text, the kind most of the",
      "document is set in, so that its size is the body size and not a heading.",
    ];
    body.forEach((l, k) => (c += `BT /F1 10 Tf 72 ${680 - 14 * k} Td (${l}) Tj ET\n`));
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << ${font} >> >> ` +
        `/Contents ${pageObj(i) + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${c.length} >>\nstream\n${c}\nendstream`);
  }
  const kids = Array.from({ length: nPages }, (_, i) => `${pageObj(i)} 0 R`).join(" ");
  objs[1] = `<< /Type /Pages /Kids [${kids}] /Count ${nPages} >>`;
  const firstItem = 4 + nPages * 2;
  const item = (k: number) => firstItem + k;
  chapters.forEach((pageIdx, k) => {
    const prev = k > 0 ? ` /Prev ${item(k - 1)} 0 R` : "";
    const next = k + 1 < chapters.length ? ` /Next ${item(k + 1)} 0 R` : "";
    objs.push(
      `<< /Title (Chapter ${k + 1}) /Parent 3 0 R${prev}${next} /Dest [${pageObj(pageIdx)} 0 R /XYZ 0 792 0] >>`,
    );
  });
  objs[2] = `<< /Type /Outlines /First ${item(0)} 0 R /Last ${item(chapters.length - 1)} 0 R /Count ${chapters.length} >>`;
  return assemblePdf(objs);
}

function makeEpub(chapters: string[], lang = "en"): Buffer {
  const enc = new TextEncoder();
  const items = chapters.map((_, i) => `<item id="c${i}" href="c${i}.xhtml" media-type="application/xhtml+xml"/>`);
  const spine = chapters.map((_, i) => `<itemref idref="c${i}"/>`);
  const opf =
    '<?xml version="1.0" encoding="utf-8"?>\n<package xmlns="http://www.idpf.org/2007/opf" version="3.0" ' +
    'unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">t</dc:identifier>' +
    `<dc:title>t</dc:title><dc:language>${lang}</dc:language></metadata>` +
    `<manifest>${items.join("")}</manifest><spine>${spine.join("")}</spine></package>`;
  const container =
    '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">' +
    '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>';
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    ...chapters.map((body, i) => ({
      name: `OEBPS/c${i}.xhtml`,
      data: enc.encode(
        `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml"><head><title>c</title></head>` +
          `<body>${body}</body></html>`,
      ),
    })),
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
  ]);
}

// an XPS page with a drawing and no text
function makeVectorXps(): Buffer {
  const enc = new TextEncoder();
  const ct =
    '<?xml version="1.0" encoding="utf-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">' +
    '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' +
    '<Default Extension="fdseq" ContentType="application/vnd.ms-package.xps-fixeddocumentsequence+xml"/>' +
    '<Default Extension="fdoc" ContentType="application/vnd.ms-package.xps-fixeddocument+xml"/>' +
    '<Default Extension="fpage" ContentType="application/vnd.ms-package.xps-fixedpage+xml"/></Types>';
  const rels =
    '<?xml version="1.0" encoding="utf-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' +
    '<Relationship Id="R0" Type="http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" Target="/FixedDocSeq.fdseq"/>' +
    "</Relationships>";
  const seq =
    '<FixedDocumentSequence xmlns="http://schemas.microsoft.com/xps/2005/06"><DocumentReference Source="/Doc.fdoc"/>' +
    "</FixedDocumentSequence>";
  const doc =
    '<FixedDocument xmlns="http://schemas.microsoft.com/xps/2005/06"><PageContent Source="/1.fpage"/></FixedDocument>';
  const page =
    '<FixedPage xmlns="http://schemas.microsoft.com/xps/2005/06" Width="816" Height="1056" xml:lang="en">' +
    '<Path Data="M 100,100 L 700,100 L 400,600 Z" Fill="#FF336699"/></FixedPage>';
  return makeZip([
    { name: "[Content_Types].xml", data: enc.encode(ct) },
    { name: "_rels/.rels", data: enc.encode(rels) },
    { name: "FixedDocSeq.fdseq", data: enc.encode(seq) },
    { name: "Doc.fdoc", data: enc.encode(doc) },
    { name: "1.fpage", data: enc.encode(page) },
  ]);
}

// text_outline_headings_and_paragraphs, page_selection_markers_and_range
function testOutlinedPdf(pdf: string): void {
  let r = cli(["markdown", pdf, "--json"]);
  expect("markdown", r.code === 0, r);
  const j = JSON.parse(r.out);
  const md: string = j.markdown;
  expect(`pages ${j.pages}`, j.pages === 40, r);
  expect("one newline at the end", md.endsWith("\n") && !md.endsWith("\n\n"), md);
  const ls = md.split("\n");
  expect(
    "# Chapter 1",
    ls.some((l) => l === "# Chapter 1" || l === "## Chapter 1"),
    md,
  );
  for (const n of [1, 17, 40]) {
    const line = ls.find((l) => l.includes(needle(n)));
    expect(`${needle(n)} in a paragraph`, line !== undefined && !line.startsWith("#"), md);
  }
  const headings = ls.filter((l) => l.startsWith("#")).length;
  const paragraphs = ls.filter((l) => l !== "" && !l.startsWith("#") && !l.startsWith("<!--")).length;
  expect(`paragraphs ${paragraphs} > headings ${headings}`, paragraphs > headings, md);

  // our --pages is a sorted range, unlike Kitab's list of pages in its order
  r = cli(["markdown", pdf, "--pages", "17,3", "--json"]);
  const sel = JSON.parse(r.out);
  expect("2 pages", sel.pages === 2, r);
  expect(
    "markers 3 and 17",
    sel.markdown.includes("<!-- page 3 -->") && sel.markdown.includes("<!-- page 17 -->"),
    sel.markdown,
  );
  expect("needle 17, not 1", sel.markdown.includes(needle(17)) && !sel.markdown.includes(needle(1)), sel.markdown);
  r = cli(["markdown", pdf, "--pages", "41"]);
  expect("page 41 of 40 is 2", r.code === 2 && r.err.includes("1-40"), r);
  r = cli(["text", pdf, "--pages", "99"]);
  expect("text --pages 99 is 2", r.code === 2, r);
}

// plain_text_has_needles. Unlike Kitab, our text separates pages with a form feed
function testPlainText(pdf: string): void {
  let r = cli(["text", pdf]);
  expect("text", r.code === 0 && r.out.includes(needle(1)) && r.out.includes(needle(40)), r);
  expect(
    "no line ends in a space",
    r.out.split("\n").every((l) => l === l.trimEnd()),
    r,
  );
  expect("one newline at the end", r.out.endsWith("\n") && !r.out.endsWith("\n\n"), r);
  r = cli(["text", pdf, "--pages", "17"]);
  expect("text --pages 17", r.out.includes(needle(17)) && !r.out.includes(needle(16)), r);
}

// pictures_saved_and_linked_or_left_out
function testPictures(): void {
  const src = join(ROOT, "tests", "issue-6132.pdf");
  const dir = tmpPath("cli-markdown-docs-pics");
  rmSync(dir, { recursive: true, force: true });
  const md = join(dir, "doc.md");
  let r = cli(["markdown", src, "-o", md, "--no-page-markers", "--json"]);
  expect("markdown -o", r.code === 0, r);
  const images: string[] = JSON.parse(r.out).images;
  expect(`${images.length} pictures`, images.length >= 3, r);
  const links = readFileSync(md, "utf8")
    .split("\n")
    .filter((l) => l.startsWith("![](doc_images/"));
  expect("a link per picture", links.length === images.length, links.join("\n"));
  for (const link of links) {
    const file = join(dir, link.slice("![](".length, -1));
    expect(`${file} is a PNG`, readFileSync(file).subarray(0, 4).equals(Buffer.from("\x89PNG", "latin1")), link);
  }
  r = cli(["markdown", src]);
  expect("without -o no pictures", r.code === 0 && !r.out.includes("!["), r);
  const withoutImages = join(dir, "none.md");
  r = cli(["markdown", src, "-o", withoutImages, "--no-images"]);
  expect("--no-images", r.code === 0 && !existsSync(join(dir, "none_images")), r);
}

function testManPage(): void {
  const man = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  let r = cli(["markdown", man]);
  expect("man page", r.code === 0 && !r.out.includes("\0"), r);
  expect("page markers", r.out.startsWith("<!-- page 1 -->") && r.out.includes("<!-- page 2 -->"), r);
  expect("same-size bold lines are headings", /^# NAME$/m.test(r.out) && /^# DESCRIPTION$/m.test(r.out), r);
  expect("emphasis keeps its spaces", r.out.includes("\\[see *zlib.h* for full description\\]"), r);

  r = cli(["markdown", man, "--pages", "2", "--no-page-markers", "--json"]);
  expect("page range as JSON", r.code === 0, r);
  const result = JSON.parse(r.out);
  expect("one page without markers", result.pages === 1 && !result.markdown.includes("<!-- page"), r);
}

// epub_converts_with_needles
function testEpub(): void {
  const chapters = [1, 2, 3, 4, 5].map(
    (n) => `<h1>Chapter ${n}</h1><p>The text of chapter ${n}<sup>${n}</sup>, with sumatra-epub-needle-${n} in it.</p>`,
  );
  const path = tmpPath("cli-markdown-docs.epub");
  writeFileSync(path, makeEpub(chapters));
  const r = cli(["markdown", path]);
  for (let n = 1; n <= 5; n++) {
    expect(`epub needle ${n}`, r.out.includes(`sumatra-epub-needle-${n}`), r);
  }
  expect(
    "headings",
    r.out.split("\n").some((l) => l.startsWith("#")),
    r,
  );
  // not in Kitab: a footnote mark isn't glued to the word
  expect("<sup>", r.out.includes("chapter 3<sup>3</sup>,"), r);

  const outlined = cli(["markdown", join(ROOT, "tests", "issue-6095.epub")]);
  expect(
    "a title from the EPUB outline is a heading",
    outlined.code === 0 && /^# Dzikie ziele$/m.test(outlined.out),
    outlined,
  );
}

function testOtherFormats(): void {
  // a page that is a drawing: no text and no error
  const xps = tmpPath("cli-markdown-docs.xps");
  writeFileSync(xps, makeVectorXps());
  let r = cli(["markdown", xps]);
  expect("xps", r.code === 0, r);

  r = cli(["markdown", join(ROOT, "tests", "issue-2254.fb2")]);
  expect("fb2", r.code === 0 && r.out.replace(/<!--[^>]*-->/g, "").trim().length > 10, r);

  const rtl = tmpPath("cli-markdown-docs-rtl.epub");
  writeFileSync(rtl, makeEpub(['<p dir="rtl">مرحبا بالعالم، هذا نص عربي للتجربة.</p>'], "ar"));
  r = cli(["markdown", rtl]);
  expect("right-to-left text", r.code === 0 && r.out.includes("مرحبا"), r);

  const notPdf = tmpPath("cli-markdown-docs-not-a.pdf");
  writeFileSync(notPdf, "this is not a PDF at all");
  expect("not a PDF is 3", cli(["markdown", notPdf]).code === 3, "");
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("cli-markdown-docs.pdf");
  writeFileSync(pdf, makeOutlinedPdf(), "latin1");
  testOutlinedPdf(pdf);
  testPlainText(pdf);
  testManPage();
  testPictures();
  testEpub();
  testOtherFormats();
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
