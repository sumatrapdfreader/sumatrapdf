// Test for issue #2199: "Better handle invalid files inside cbx/cbr file".
//
// A file inside a .cbz that isn't a decodable image used to render as an empty
// page (black, the comic book background) with nothing telling the reader that
// the page is broken -- it just looked like a blank page of the comic.
// EngineImages::RenderPage() handed back a blank bitmap instead of failing, so
// the "Couldn't render page N" message the canvas already draws for failed
// pages never appeared.
//
// The test builds a .cbz whose middle page is garbage, goes to that page and
// checks that something is painted in the middle of the otherwise black page
// area (the message). Pages 1 and 3 must still render normally.

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { ControlCommand } from "./control.ts";
import { tmpPath } from "./util";
import { killAndWait, launchControlled } from "./win-automation";

function crc32(buf: Buffer): number {
  let c;
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

// a valid solid-color truecolor PNG
function makePng(w: number, h: number, rgb: [number, number, number]): Buffer {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) {
    const row = y * (w * 3 + 1);
    raw[row] = 0; // filter: none
    for (let x = 0; x < w; x++) {
      raw[row + 1 + x * 3] = rgb[0];
      raw[row + 2 + x * 3] = rgb[1];
      raw[row + 3 + x * 3] = rgb[2];
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 2; // color type: truecolor
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

// zip with stored (uncompressed) entries
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

export async function testit(): Promise<void> {
  const cbz = tmpPath("issue-2199.cbz");
  writeFileSync(
    cbz,
    makeZip([
      { name: "page1.png", data: makePng(300, 400, [220, 40, 40]) },
      // not a decodable image, though it claims to be a .png
      { name: "page2.png", data: Buffer.alloc(4096, 0x41) },
      { name: "page3.png", data: makePng(300, 400, [40, 80, 220]) },
    ]),
  );

  const { proc, client } = await launchControlled([cbz]);
  try {
    await client.waitForRenderIdle();
    const p1 = await client.request(ControlCommand.TestRenderPageColors, [cbz, 1]);
    const p1Raw = String(p1[1] ?? "");
    if (p1[0] !== 0 || !/red=\d+/.test(p1Raw)) {
      throw new Error(`issue-2199 page 1 should render: ${p1Raw.trim()}`);
    }

    const p2 = await client.request(ControlCommand.TestRenderPageColors, [cbz, 2]);
    const p2Raw = String(p2[1] ?? "");
    if (p2[0] === 0 || !/render-failed/.test(p2Raw)) {
      throw new Error(`issue-2199: broken cbz page rendered instead of failing: ${p2Raw.trim()}`);
    }

    const p3 = await client.request(ControlCommand.TestRenderPageColors, [cbz, 3]);
    const p3Raw = String(p3[1] ?? "");
    if (p3[0] !== 0) {
      throw new Error(`issue-2199 page 3 stopped rendering after the broken page: ${p3Raw.trim()}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
