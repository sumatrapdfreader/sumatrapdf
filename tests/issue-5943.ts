// #5943: EPUB TOC entries whose ids sit on nested <span>s wrapping a block
// all resolved to the chapter's first page. MuPDF kept appending to the same
// interrupted inline context; upstream 37a14e17 (bug 709648) starts a new one.
//
// Run: bun tests/issue-5943.ts [--no-build]

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
  const all = [...chunks, ...central, new Uint8Array(end.buffer)];
  const total = all.reduce((n, c) => n + c.length, 0);
  const out = Buffer.alloc(total);
  let p = 0;
  for (const c of all) {
    out.set(c, p);
    p += c.length;
  }
  return out;
}

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const filler = (n: number) =>
    Array.from({ length: 60 }, (_, i) => `<p>Filler paragraph ${n}.${i + 1} of section ${n}.</p>`).join("\n");

  const body: string[] = [];
  const nav: string[] = [];
  let playOrder = 0;
  const navPoint = (label: string, id: string) => {
    playOrder++;
    nav.push(
      `    <navPoint id="np${playOrder}" playOrder="${playOrder}">` +
        `<navLabel><text>${label}</text></navLabel>` +
        `<content src="ch.xhtml#${id}"/></navPoint>`,
    );
  };

  body.push(`<span id="outer"><div class="h">Outer</div>`);
  navPoint("outer (span)", "outer");
  for (let n = 1; n <= 3; n++) {
    body.push(`<span id="span${n}"><div class="h">Span-anchored section ${n}</div>`);
    body.push(filler(n));
    body.push(`<div id="div${n}" class="h">Div-anchored section ${n}</div>`);
    body.push(filler(n + 10));
    body.push(`</span>`);
    navPoint(`span-anchored section ${n}`, `span${n}`);
    navPoint(`div-anchored section ${n}`, `div${n}`);
  }
  body.push(`</span>`);

  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">span-anchor-repro</dc:identifier>` +
    `<dc:title>span anchor repro</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>` +
    `<item id="ch" href="ch.xhtml" media-type="application/xhtml+xml"/>` +
    `</manifest><spine toc="ncx"><itemref idref="ch"/></spine></package>`;
  const ncx =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">` +
    `<head><meta name="dtb:uid" content="span-anchor-repro"/></head>` +
    `<docTitle><text>span anchor repro</text></docTitle>` +
    `<navMap>\n${nav.join("\n")}\n</navMap></ncx>`;
  const html =
    `<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE html>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>chapter</title></head>` +
    `<body>\n${body.join("\n")}\n</body></html>`;

  return zip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/book.opf", data: enc.encode(opf) },
    { name: "OEBPS/toc.ncx", data: enc.encode(ncx) },
    { name: "OEBPS/ch.xhtml", data: enc.encode(html) },
  ]);
}

function parsePages(toc: string): Map<string, number> {
  const out = new Map<string, number>();
  for (const line of toc.split("\n")) {
    const m = /^(.*)\|page=(\d+)$/.exec(line);
    if (m) {
      out.set(m[1]!.trim(), +m[2]!);
    }
  }
  return out;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5943");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "repro.epub");
  writeFileSync(epub, makeEpub());

  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestGetToc, [epub]);
  if (exitCode !== 0) {
    throw new Error(`issue-5943: TestGetToc failed: ${String(raw ?? "").trim()}`);
  }
  const pages = parsePages(String(raw ?? ""));
  const span = [1, 2, 3].map((n) => pages.get(`span-anchored section ${n}`));
  const div = [1, 2, 3].map((n) => pages.get(`div-anchored section ${n}`));
  if (span.some((p) => p == null) || div.some((p) => p == null)) {
    throw new Error(`issue-5943: missing TOC entries:\n${raw}`);
  }
  if (!(span[0]! < span[1]! && span[1]! < span[2]!)) {
    throw new Error(
      `issue-5943: nested span ids all land on the same page ` +
        `(span=${span.join(",")} div=${div.join(",")})\n${raw}`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
