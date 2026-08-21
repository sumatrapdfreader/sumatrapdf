// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3415
//
// WebP already opens as a standalone image / CBZ page. Inside an EPUB, MuPDF
// used to fail fz_new_image_from_buffer and draw the IMAGE placeholder.
// This packs a solid-red 80x80 WebP into a one-page EPUB and checks that
// rendering page 1 paints a block of red.
//
// Run:  bun tests/issue-3415.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { deflateRawSync } from "node:zlib";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// 80x80 lossy WebP, solid (220,0,0). Generated with Pillow.
const RED_WEBP = Buffer.from(
  "UklGRogAAABXRUJQVlA4IHwAAACQCgCdASpQAFAAPjEYi0QiIaEQpAAgAwS0gDsAfgAZHy/XQ+Sq51Oag0RmM2sd7lgTdgvOxAwq/15A9eSzVg7ARCiwThz6Hylhz6AdgIhRYJybIc8Fs59AOwEQm4AA/v/xz1f/+mkeNI8aR9Jv//90CfuPL9x5f+5tAAAA",
  "base64",
);

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

function makeEpub(webp: Buffer): Buffer {
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
    `<dc:identifier id="id">urn:uuid:issue-3415</dc:identifier>` +
    `<dc:title>webp in epub</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="img" href="red.webp" media-type="image/webp"/>` +
    `</manifest><spine><itemref idref="c1"/></spine></package>`;
  const html =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title></head>` +
    `<body style="margin:0"><img src="red.webp" width="80" height="80" alt="red"/></body></html>`;
  return zip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(html) },
    { name: "OEBPS/red.webp", data: webp, store: true },
  ]);
}

function parseColors(raw: string): { red: number; nonWhite: number; w: number; h: number } {
  const m = /red=(\d+) nonwhite=(\d+) size=(\d+)x(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-3415: could not parse: ${raw}`);
  }
  return { red: +m[1]!, nonWhite: +m[2]!, w: +m[3]!, h: +m[4]! };
}

export async function testit(): Promise<void> {
  const webpPath = tmpPath("issue-3415.webp");
  const epubPath = tmpPath("issue-3415.epub");
  writeFileSync(webpPath, RED_WEBP);
  writeFileSync(epubPath, makeEpub(RED_WEBP));

  await withControlledSumatra(EXE, async (client) => {
    {
      const res = await client.request(ControlCommand.TestRenderPageColors, [webpPath]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-3415 standalone webp: ${raw.trim()}`);
      }
      const c = parseColors(raw);
      if (c.red < 200) {
        throw new Error(`issue-3415 standalone webp: red=${c.red}, want many:\n${raw}`);
      }
      console.log(`  standalone webp: ${c.w}x${c.h} red=${c.red} ✓`);
    }
    {
      const res = await client.request(ControlCommand.TestRenderPageColors, [epubPath]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-3415 epub: ${raw.trim()}`);
      }
      const c = parseColors(raw);
      if (c.red < 200) {
        throw new Error(`issue-3415 epub: red=${c.red} (WebP still a placeholder?):\n${raw}`);
      }
      console.log(`  epub webp: ${c.w}x${c.h} red=${c.red} ✓`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
