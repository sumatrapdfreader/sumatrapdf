// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5803
//
// EPUB spine items with linear="no" (footnotes, asides) must be omitted from
// the default reading order. "CHAPTEROK" is the linear chapter;
// "FOOTNOTESECRET" is the nonlinear item.
//
// Run:  bun tests/issue-5803.ts [--no-build]   (or via tests/all.ts)

import { writeFileSync } from "node:fs";
import { extractPageText, runStandalone, tmpPath } from "./util.ts";

function crc32(data: Buffer): number {
  let c = 0xffffffff;
  for (let i = 0; i < data.length; i++) {
    c ^= data[i]!;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
  }
  return (c ^ 0xffffffff) >>> 0;
}

// stored (uncompressed) zip, mimetype first as EPUB requires
function makeZip(entries: { name: string; data: Buffer }[]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "latin1");
    const crc = crc32(e.data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0);
    lh.writeUInt16LE(20, 4);
    lh.writeUInt32LE(crc, 14);
    lh.writeUInt32LE(e.data.length, 18);
    lh.writeUInt32LE(e.data.length, 22);
    lh.writeUInt16LE(name.length, 26);
    locals.push(lh, name, e.data);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0);
    ch.writeUInt16LE(20, 4);
    ch.writeUInt16LE(20, 6);
    ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(e.data.length, 20);
    ch.writeUInt32LE(e.data.length, 24);
    ch.writeUInt16LE(name.length, 28);
    ch.writeUInt32LE(offset, 42);
    centrals.push(ch, name);
    offset += 30 + name.length + e.data.length;
  }
  const localBuf = Buffer.concat(locals);
  const centralBuf = Buffer.concat(centrals);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralBuf.length, 12);
  end.writeUInt32LE(localBuf.length, 16);
  return Buffer.concat([localBuf, centralBuf, end]);
}

function xhtml(body: string): Buffer {
  return Buffer.from(
    `<?xml version="1.0" encoding="utf-8"?>\n` +
      `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title></head>` +
      `<body><p>${body}</p></body></html>\n`,
    "utf8",
  );
}

function makeEpub(notesLinearNo: boolean): Buffer {
  const container = Buffer.from(
    `<?xml version="1.0" encoding="utf-8"?>\n` +
      `<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">` +
      `<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
      `</rootfiles></container>\n`,
    "utf8",
  );
  const notesRef = notesLinearNo ? `<itemref idref="notes" linear="no"/>` : `<itemref idref="notes"/>`;
  const opf = Buffer.from(
    `<?xml version="1.0" encoding="utf-8"?>\n` +
      `<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">` +
      `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
      `<dc:identifier id="id">issue-5803</dc:identifier><dc:title>t</dc:title>` +
      `</metadata>` +
      `<manifest>` +
      `<item id="ch" href="ch.xhtml" media-type="application/xhtml+xml"/>` +
      `<item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/>` +
      `</manifest>` +
      `<spine toc="">` +
      `<itemref idref="ch"/>` +
      notesRef +
      `</spine></package>\n`,
    "utf8",
  );
  return makeZip([
    { name: "mimetype", data: Buffer.from("application/epub+zip", "latin1") },
    { name: "META-INF/container.xml", data: container },
    { name: "OEBPS/content.opf", data: opf },
    { name: "OEBPS/ch.xhtml", data: xhtml("CHAPTEROK") },
    { name: "OEBPS/notes.xhtml", data: xhtml("FOOTNOTESECRET") },
  ]);
}

export async function testit(): Promise<void> {
  const withNotes = tmpPath("issue-5803-linear.epub");
  const withoutNotes = tmpPath("issue-5803-nonlinear.epub");
  writeFileSync(withNotes, makeEpub(false));
  writeFileSync(withoutNotes, makeEpub(true));

  const all = extractPageText(withNotes);
  if (!all.includes("CHAPTEROK")) {
    throw new Error(`issue-5803: linear epub missing chapter text: ${JSON.stringify(all)}`);
  }
  if (!all.includes("FOOTNOTESECRET")) {
    throw new Error(`issue-5803: linear epub should include the notes item: ${JSON.stringify(all)}`);
  }

  const skipped = extractPageText(withoutNotes);
  if (!skipped.includes("CHAPTEROK")) {
    throw new Error(`issue-5803: nonlinear epub missing chapter text: ${JSON.stringify(skipped)}`);
  }
  if (skipped.includes("FOOTNOTESECRET")) {
    throw new Error(`issue-5803: linear="no" notes still in reading order: ${JSON.stringify(skipped)}`);
  }
  console.log("issue-5803: linear=no spine items skipped from reading order");
}

if (import.meta.main) {
  await runStandalone(testit);
}
