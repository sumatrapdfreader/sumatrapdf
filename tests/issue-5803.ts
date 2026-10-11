// issue #5803: an EPUB spine item with linear="no" (footnotes, endnotes) is
// not part of the reading order. mupdf paged it where it sat in the spine, so
// a notes page interrupted the chapters. It now goes after the linear items,
// where links still reach it.
//
// An XHTML manifest item no itemref names (an aside only a link reaches) is
// paged after those; a nav document outside the spine is not.
//
// The EPUB is generated: chapter one, the notes (linear="no"), chapter two,
// an extra page and a nav document outside the spine.
// Run: bun tests/issue-5803.ts [--no-build]
import { writeFileSync } from "node:fs";
import { extractPageText, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

function xhtml(title: string, body: string): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>${title}</title></head>` +
    `<body><h1>${title}</h1>${body}</body></html>`
  );
}

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">issue-5803</dc:identifier>` +
    `<dc:title>Linear no</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="c2" href="c2.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="extra" href="extra.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>` +
    `<item id="css" href="style.css" media-type="text/css"/>` +
    `</manifest><spine>` +
    `<itemref idref="c1"/>` +
    `<itemref idref="notes" linear="no"/>` +
    `<itemref idref="c2"/>` +
    `</spine></package>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(xhtml("CHAPTERONE", `<p>Text<a href="notes.xhtml#n1">1</a></p>`)) },
    { name: "OEBPS/notes.xhtml", data: enc.encode(xhtml("NOTES", `<p id="n1">A note</p>`)) },
    { name: "OEBPS/c2.xhtml", data: enc.encode(xhtml("CHAPTERTWO", `<p>More text<a href="extra.xhtml">*</a></p>`)) },
    // in the manifest, not in the spine: reachable through the link only
    { name: "OEBPS/extra.xhtml", data: enc.encode(xhtml("EXTRA", `<p>An aside</p>`)) },
    // a nav document outside the spine is not a page
    {
      name: "OEBPS/nav.xhtml",
      data: enc.encode(xhtml("NAVTOC", `<nav><ol><li><a href="c1.xhtml">1</a></li></ol></nav>`)),
    },
    { name: "OEBPS/style.css", data: enc.encode("p { margin: 0 }") },
  ];
  return makeZip(entries);
}

export async function testit(): Promise<void> {
  const epub = tmpPath("issue-5803.epub");
  writeFileSync(epub, makeEpub());

  // every page, in page order
  const text = extractPageText(epub);
  const expected = ["CHAPTERONE", "CHAPTERTWO", "NOTES", "EXTRA"];
  let last = -1;
  for (const title of expected) {
    const at = text.indexOf(title);
    if (at <= last) {
      throw new Error(`#5803: pages should read ${expected.join(", ")}, got:\n${JSON.stringify(text)}`);
    }
    last = at;
  }
  if (text.includes("NAVTOC")) {
    throw new Error("#5803: the nav document outside the spine became a page");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
