// Builds tests/issue-chm-lzx.chm: a minimal CHM with malformed LZX PRETREE lengths.
// Regenerate: bun tests/issue-chm-lzx-make.ts

import { writeFileSync } from "node:fs";
import { join } from "node:path";

const OUT = join(import.meta.dir, "issue-chm-lzx.chm");

function encodeCword(v: number): number[] {
  if (v < 0) throw new Error(`negative cword ${v}`);
  const chunks: number[] = [];
  let n = v;
  do {
    chunks.push(n & 0x7f);
    n >>= 7;
  } while (n > 0);
  const out: number[] = [];
  for (let i = chunks.length - 1; i > 0; i--) out.push(chunks[i] | 0x80);
  out.push(chunks[0]);
  return out;
}

function pmglEntry(path: string, space: number, start: number, length: number): number[] {
  const bytes = [...path].map((c) => c.charCodeAt(0));
  return [...encodeCword(bytes.length), ...bytes, ...encodeCword(space), ...encodeCword(start), ...encodeCword(length)];
}

// LZX bitstream writer matching the LZX decoder in ext/libchm/chm.c READ_BITS (32-bit buffer, 16-bit input words LE).
class LzxBitWriter {
  private bits: number[] = [];

  write(value: number, nbits: number) {
    for (let i = nbits - 1; i >= 0; i--) this.bits.push((value >> i) & 1);
  }

  toBytes(): Uint8Array {
    while (this.bits.length % 16 !== 0) this.bits.push(0);
    const out = new Uint8Array(this.bits.length / 8);
    for (let i = 0; i < out.length; i++) {
      let b = 0;
      for (let j = 0; j < 8; j++) b |= this.bits[i * 8 + j] << j;
      out[i] = b;
    }
    return out;
  }
}

function buildMaliciousLzxBlock(): Uint8Array {
  const w = new LzxBitWriter();
  // LZX header: no intel transform
  w.write(0, 1);
  // verbatim block
  w.write(1, 3);
  // block length 0x100 (enough to enter READ_LENGTHS)
  w.write(0, 16);
  w.write(0x01, 8);
  // lzx_read_lens: 20 pretree lengths, all 15 (max from 4-bit read)
  for (let i = 0; i < 20; i++) w.write(15, 4);
  // padding for MAINTREE length reads (will fail after pretree BUILD_TABLE)
  for (let i = 0; i < 64; i++) w.write(0, 4);
  return w.toBytes();
}

function u32le(v: number) {
  const b = new Uint8Array(4);
  const view = new DataView(b.buffer);
  view.setUint32(0, v >>> 0, true);
  return b;
}

function u64le(v: number) {
  const b = new Uint8Array(8);
  const view = new DataView(b.buffer);
  view.setBigUint64(0, BigInt(v), true);
  return b;
}

function cat(...parts: Uint8Array[]): Uint8Array {
  const n = parts.reduce((s, p) => s + p.length, 0);
  const out = new Uint8Array(n);
  let off = 0;
  for (const p of parts) {
    out.set(p, off);
    off += p.length;
  }
  return out;
}

function buildChm(): Uint8Array {
  const BLOCK_LEN = 0x800;
  const HEADER_LEN = 0x60;

  const resetTable = cat(
    u32le(2), // version
    u32le(1), // block_count
    u32le(0), // unknown
    u32le(0x10), // table_offset
    u64le(0x8000), // uncompressed_len (32KiB block)
    u64le(0), // compressed_len (patched below)
    u64le(0x8000), // block_len
    u64le(0), // block 0 start
    u64le(0), // block end placeholder
  );

  const controlData = cat(
    u32le(0x1c),
    new TextEncoder().encode("LZXC"),
    u32le(2), // version 2
    u32le(1), // resetInterval (*0x8000)
    u32le(1), // windowSize (*0x8000 => 32768)
    u32le(1), // windowsPerReset
    u32le(0),
  );

  const lzx = buildMaliciousLzxBlock();
  const compressedLen = lzx.length;

  // patch compressed_len in reset table (offset 0x18)
  const rt = new Uint8Array(resetTable);
  new DataView(rt.buffer).setBigUint64(0x18, BigInt(compressedLen), true);
  new DataView(rt.buffer).setBigUint64(0x28, BigInt(compressedLen), true);

  const systemData = new Uint8Array([
    0x02, 0x00, 0x0a, 0x00, 0x2f, 0x70, 0x61, 0x79, 0x6c, 0x6f, 0x61, 0x64,
    0x02, 0x00, 0x05, 0x00, 0x74, 0x65, 0x73, 0x74, 0x73,
  ]);

  const dataObjects = cat(rt, controlData, lzx, systemData);
  const resetOff = 0;
  const controlOff = rt.length;
  const contentOff = controlOff + controlData.length;
  const systemOff = contentOff + lzx.length;

  const entries = cat(
    new Uint8Array(
      pmglEntry(
        "::DataSpace/Storage/MSCompressed/Transform/{7FC28940-9D31-11D0-9B27-00A0C91E9C7C}/InstanceData/ResetTable",
        0,
        resetOff,
        rt.length,
      ),
    ),
    new Uint8Array(pmglEntry("::DataSpace/Storage/MSCompressed/ControlData", 0, controlOff, controlData.length)),
    new Uint8Array(pmglEntry("::DataSpace/Storage/MSCompressed/Content", 0, contentOff, compressedLen)),
    new Uint8Array(pmglEntry("/payload", 1, 0, 1)),
    new Uint8Array(pmglEntry("/#SYSTEM", 0, systemOff, systemData.length)),
    new Uint8Array(pmglEntry("/index.html", 1, 0, 1)),
  );

  const pmgl = new Uint8Array(BLOCK_LEN);
  pmgl.set(new TextEncoder().encode("PMGL"), 0);
  const view = new DataView(pmgl.buffer);
  const used = 0x14 + entries.length;
  view.setUint32(4, BLOCK_LEN - used, true);
  view.setUint32(8, 0, true);
  view.setInt32(12, -1, true);
  view.setInt32(16, -1, true);
  pmgl.set(entries, 0x14);

  const itsp = new Uint8Array(0x54);
  itsp.set(new TextEncoder().encode("ITSP"), 0);
  const itspView = new DataView(itsp.buffer);
  itspView.setInt32(4, 1, true);
  itspView.setInt32(8, 0x54, true);
  itspView.setUint32(0x10, BLOCK_LEN, true);
  itspView.setInt32(0x1c, -1, true);
  itspView.setInt32(0x20, 0, true);
  itspView.setUint32(0x28, 1, true);

  const dir = cat(itsp, pmgl);
  const dirOffset = HEADER_LEN;
  const dataOffset = dirOffset + dir.length;

  const itsf = new Uint8Array(HEADER_LEN);
  itsf.set(new TextEncoder().encode("ITSF"), 0);
  const h = new DataView(itsf.buffer);
  h.setInt32(4, 3, true);
  h.setInt32(8, HEADER_LEN, true);
  h.setBigUint64(0x48, BigInt(dirOffset), true);
  h.setBigUint64(0x50, BigInt(dir.length), true);
  h.setBigUint64(0x58, BigInt(dataOffset), true);

  return cat(itsf, dir, dataObjects);
}

const chm = buildChm();
writeFileSync(OUT, chm);
console.log(`wrote ${OUT} (${chm.length} bytes)`);