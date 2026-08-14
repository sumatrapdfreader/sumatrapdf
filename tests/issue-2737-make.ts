// Generates tests/issue-2737.chm, a minimal uncompressed CHM with a legacy
// <font face> declaration. Regenerate with: bun tests/issue-2737-make.ts

import { writeFileSync } from "node:fs";
import { join } from "node:path";

const OUT = join(import.meta.dir, "issue-2737.chm");
const enc = new TextEncoder();

function u16le(v: number): Uint8Array {
  const b = new Uint8Array(2);
  new DataView(b.buffer).setUint16(0, v, true);
  return b;
}

function u32le(v: number): Uint8Array {
  const b = new Uint8Array(4);
  new DataView(b.buffer).setUint32(0, v, true);
  return b;
}

function u64le(v: number): Uint8Array {
  const b = new Uint8Array(8);
  new DataView(b.buffer).setBigUint64(0, BigInt(v), true);
  return b;
}

function cat(...parts: Uint8Array[]): Uint8Array {
  const out = new Uint8Array(parts.reduce((n, part) => n + part.length, 0));
  let off = 0;
  for (const part of parts) {
    out.set(part, off);
    off += part.length;
  }
  return out;
}

function cword(v: number): Uint8Array {
  const chunks: number[] = [];
  do {
    chunks.push(v & 0x7f);
    v >>= 7;
  } while (v > 0);
  const out: number[] = [];
  for (let i = chunks.length - 1; i > 0; i--) out.push(chunks[i]! | 0x80);
  out.push(chunks[0]!);
  return new Uint8Array(out);
}

function pmglEntry(path: string, start: number, size: number): Uint8Array {
  const name = enc.encode(path);
  return cat(cword(name.length), name, cword(0), cword(start), cword(size));
}

function systemRecord(type: number, value: string): Uint8Array {
  const data = cat(enc.encode(value), new Uint8Array([0]));
  return cat(u16le(type), u16le(data.length), data);
}

function buildChm(): Uint8Array {
  const blockSize = 0x800;
  const headerSize = 0x60;
  const system = cat(u32le(3), systemRecord(2, "/index.html"), systemRecord(3, "Issue 2737"));
  const html = enc.encode(
    '<html><body><font face="Courier New">CHM font override regression test</font></body></html>',
  );
  const data = cat(system, html);
  const entries = cat(pmglEntry("/#SYSTEM", 0, system.length), pmglEntry("/index.html", system.length, html.length));

  const pmgl = new Uint8Array(blockSize);
  pmgl.set(enc.encode("PMGL"), 0);
  const pmglView = new DataView(pmgl.buffer);
  const used = 0x14 + entries.length;
  pmglView.setUint32(4, blockSize - used, true);
  pmglView.setUint32(8, 0, true);
  pmglView.setInt32(12, -1, true);
  pmglView.setInt32(16, -1, true);
  pmgl.set(entries, 0x14);

  const itsp = new Uint8Array(0x54);
  itsp.set(enc.encode("ITSP"), 0);
  const itspView = new DataView(itsp.buffer);
  itspView.setInt32(4, 1, true);
  itspView.setInt32(8, 0x54, true);
  itspView.setUint32(0x10, blockSize, true);
  itspView.setInt32(0x1c, -1, true);
  itspView.setInt32(0x20, 0, true);
  itspView.setUint32(0x28, 1, true);

  const directory = cat(itsp, pmgl);
  const dataOffset = headerSize + directory.length;
  const itsf = new Uint8Array(headerSize);
  itsf.set(enc.encode("ITSF"), 0);
  const itsfView = new DataView(itsf.buffer);
  itsfView.setInt32(4, 3, true);
  itsfView.setInt32(8, headerSize, true);
  itsfView.setBigUint64(0x48, BigInt(headerSize), true);
  itsfView.setBigUint64(0x50, BigInt(directory.length), true);
  itsfView.setBigUint64(0x58, BigInt(dataOffset), true);
  return cat(itsf, directory, data);
}

const chm = buildChm();
writeFileSync(OUT, chm);
console.log(`wrote ${OUT} (${chm.length} bytes)`);
