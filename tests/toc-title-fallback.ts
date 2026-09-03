// A TOC label with no letter or digit (some epubs, e.g. Dune, label every
// chapter " . ") tells the reader nothing. EngineMupdf::BuildTocTree replaces
// such a label with the item's 1-based number among its siblings.
//
// Run: bun tests/toc-title-fallback.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { deflateRawSync } from "node:zlib";
import { ControlCommand, runControlCommand } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    t[i] = c >>> 0;
  }
  return t;
})();

function crc32(buf: Uint8Array): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) {
    c = CRC_TABLE[(c ^ buf[i]!) & 0xff]! ^ (c >>> 8);
  }
  return (c ^ 0xffffffff) >>> 0;
}

type ZipEntry = { name: string; data: Uint8Array; store?: boolean };

function zip(entries: ZipEntry[]): Buffer {
  const chunks: Uint8Array[] = [];
  const central: Uint8Array[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = new TextEncoder().encode(e.name);
    const store = e.store === true;
    const body = store ? e.data : new Uint8Array(deflateRawSync(e.data));
    const crc = crc32(e.data);
    const local = new DataView(new ArrayBuffer(30));
    local.setUint32(0, 0x04034b50, true);
    local.setUint16(4, 20, true);
    local.setUint16(8, store ? 0 : 8, true);
    local.setUint32(14, crc, true);
    local.setUint32(18, body.length, true);
    local.setUint32(22, e.data.length, true);
    local.setUint16(26, name.length, true);
    chunks.push(new Uint8Array(local.buffer), name, body);

    const cd = new DataView(new ArrayBuffer(46));
    cd.setUint32(0, 0x02014b50, true);
    cd.setUint16(4, 20, true);
    cd.setUint16(6, 20, true);
    cd.setUint16(10, store ? 0 : 8, true);
    cd.setUint32(16, crc, true);
    cd.setUint32(20, body.length, true);
    cd.setUint32(24, e.data.length, true);
    cd.setUint16(28, name.length, true);
    cd.setUint32(42, offset, true);
    central.push(new Uint8Array(cd.buffer), name);
    offset += 30 + name.length + body.length;
  }
  const cdSize = central.reduce((n, c) => n + c.length, 0);
  const end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true);
  end.setUint16(8, entries.length, true);
  end.setUint16(10, entries.length, true);
  end.setUint32(12, cdSize, true);
  end.setUint32(16, offset, true);
  return Buffer.concat([...chunks, ...central, new Uint8Array(end.buffer)]);
}

// three chapters: a real label and two punctuation-only ones (Dune labels
// every chapter " . "); the last two get numbered 2 and 3. A whitespace-only
// label is not a case here: mupdf drops such navPoints before we see them.
const LABELS = ["Prologue", " . ", "- -"];

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">` +
    `<rootfiles><rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>`;
  const manifest: string[] = [`<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>`];
  const spine: string[] = [];
  const nav: string[] = [];
  const files: ZipEntry[] = [];
  LABELS.forEach((label, i) => {
    const n = i + 1;
    manifest.push(`<item id="ch${n}" href="ch${n}.xhtml" media-type="application/xhtml+xml"/>`);
    spine.push(`<itemref idref="ch${n}"/>`);
    nav.push(
      `<navPoint id="np${n}" playOrder="${n}"><navLabel><text>${label}</text></navLabel>` +
        `<content src="ch${n}.xhtml"/></navPoint>`,
    );
    const html =
      `<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE html>\n` +
      `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>chapter ${n}</title></head>` +
      `<body><p>Chapter ${n} body text.</p></body></html>`;
    files.push({ name: `OEBPS/ch${n}.xhtml`, data: enc.encode(html) });
  });
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="uid" version="2.0">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">toc-title-fallback</dc:identifier>` +
    `<dc:title>toc title fallback</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>${manifest.join("")}</manifest><spine toc="ncx">${spine.join("")}</spine></package>`;
  const ncx =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">` +
    `<head><meta name="dtb:uid" content="toc-title-fallback"/></head>` +
    `<docTitle><text>toc title fallback</text></docTitle>` +
    `<navMap>\n${nav.join("\n")}\n</navMap></ncx>`;
  return zip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/book.opf", data: enc.encode(opf) },
    { name: "OEBPS/toc.ncx", data: enc.encode(ncx) },
    ...files,
  ]);
}

function parseTitles(toc: string): string[] {
  const out: string[] = [];
  for (const line of toc.split("\n")) {
    const m = /^(.*)\|page=(-?\d+)$/.exec(line);
    if (m) {
      out.push(m[1]!.trim());
    }
  }
  return out;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("toc-title-fallback");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "repro.epub");
  writeFileSync(epub, makeEpub());

  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestGetToc, [epub]);
  if (exitCode !== 0) {
    throw new Error(`toc-title-fallback: TestGetToc failed: ${String(raw ?? "").trim()}`);
  }
  const titles = parseTitles(String(raw ?? ""));
  const expected = ["Prologue", "2", "3"];
  if (JSON.stringify(titles) !== JSON.stringify(expected)) {
    throw new Error(
      `toc-title-fallback: expected titles ${JSON.stringify(expected)}, got ${JSON.stringify(titles)}\n${raw}`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
