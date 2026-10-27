// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6409
//
// `content: counters(section, ".") ". "` in a :before rendered the function
// name: "countersTITLE". A string content must still render.
//
// Run: bun tests/issue-6409.ts [--no-build]

import { writeFileSync } from "node:fs";
import { extractPageText, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const css =
  `ol.nest { counter-reset: section; list-style-type: none; }\n` +
  `li.nest:before { counter-increment: section; content: counters(section, ".") ". "; }\n` +
  `p.mark:before { content: "MARK"; }\n`;

const body =
  `<ol class="nest"><li class="nest">TITLE</li><li class="nest">CONTENTS</li></ol>` + `<p class="mark">ED</p>`;

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
    `<dc:identifier id="id">issue-6409</dc:identifier>` +
    `<dc:title>Counters</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest><item id="c" href="c.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="s" href="style.css" media-type="text/css"/></manifest>` +
    `<spine><itemref idref="c"/></spine></package>`;
  const xhtml =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>c</title>` +
    `<link rel="stylesheet" href="style.css" type="text/css"/></head><body>${body}</body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/style.css", data: enc.encode(css) },
    { name: "OEBPS/c.xhtml", data: enc.encode(xhtml) },
  ];
  return makeZip(entries);
}

export async function testit(): Promise<void> {
  const epub = tmpPath("issue-6409.epub");
  writeFileSync(epub, makeEpub());
  const text = extractPageText(epub, 1);
  if (/counters/.test(text)) {
    throw new Error(`issue-6409: the counters() function name was rendered: '${text}'`);
  }
  for (const want of ["TITLE", "CONTENTS", "MARKED"]) {
    if (!text.includes(want)) {
      throw new Error(`issue-6409: '${want}' missing from '${text}'`);
    }
  }
  console.log("issue-6409: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
