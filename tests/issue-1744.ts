// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1744
//
// Fill & Sign-style electronic signature: stamp a PNG onto a PDF page as an
// image annotation. File → Insert Image... and the document context menu run
// the same command; this test drives the engine path those menus use.
//
// Run:  bun tests/issue-1744.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, assemblePdf } from "./util.ts";

function crc32(buf: Buffer): number {
  let c: number;
  let crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    c = (crc ^ buf[n]!) & 0xff;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type: string, data: Buffer): Buffer {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, "latin1"), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

function makeRedPng(w: number, h: number): Buffer {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) {
    const row = y * (w * 3 + 1);
    raw[row] = 0;
    for (let x = 0; x < w; x++) {
      const i = row + 1 + x * 3;
      raw[i] = 220;
      raw[i + 1] = 0;
      raw[i + 2] = 0;
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function makePdf(): string {
  const content = "BT /F1 12 Tf 72 720 Td (sign here) Tj ET";
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>`,
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>`,
  ];
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-1744.pdf");
  const png = tmpPath("issue-1744-sig.png");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(png, makeRedPng(48, 24));

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestInsertImage, [pdf, png]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-1744: ${raw.trim()}`);
    }
    const m = /annots=(\d+) red=(\d+)/.exec(raw);
    if (!m) {
      throw new Error(`issue-1744: could not parse: ${raw}`);
    }
    console.log(`  stamped image: annots=${m[1]} red=${m[2]} ✓`);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
