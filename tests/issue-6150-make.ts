// Regenerates tests/issue-6150.gif: a 2-colour GIF87a with a black rectangle
// on white. Hand-rolled so the fixture stays tiny and has no external tooling.
// Run: bun tests/issue-6150-make.ts

import { join } from "node:path";

const W = 120;
const H = 60;
const MIN_CODE_SIZE = 2;
const CLEAR = 4;
const END = 5;
const CODE_BITS = 3; // stays 3 as long as the LZW table never grows past 8 entries

function makeGif(): Uint8Array {
  const out: number[] = [];
  out.push(0x47, 0x49, 0x46, 0x38, 0x37, 0x61); // "GIF87a"
  out.push(W & 0xff, W >> 8, H & 0xff, H >> 8);
  out.push(0x80, 0x00, 0x00); // global colour table, 2 entries
  out.push(0xff, 0xff, 0xff, 0x00, 0x00, 0x00); // white, black
  out.push(0x2c, 0, 0, 0, 0, W & 0xff, W >> 8, H & 0xff, H >> 8, 0x00); // image descriptor
  out.push(MIN_CODE_SIZE);

  // emit a clear code every few pixels so the table never grows and every code
  // stays CODE_BITS wide -- a real LZW compressor is not worth it here
  const bits: number[] = [];
  let buf = 0;
  let nBits = 0;
  const emit = (code: number) => {
    buf |= code << nBits;
    nBits += CODE_BITS;
    while (nBits >= 8) {
      bits.push(buf & 0xff);
      buf >>= 8;
      nBits -= 8;
    }
  };
  emit(CLEAR);
  let n = 0;
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const inBox = x > 15 && x < W - 15 && y > 10 && y < H - 10;
      emit(inBox ? 1 : 0);
      if (++n % 100 === 0) {
        emit(CLEAR);
      }
    }
  }
  emit(END);
  if (nBits > 0) {
    bits.push(buf & 0xff);
  }
  for (let i = 0; i < bits.length; i += 255) {
    const chunk = bits.slice(i, i + 255);
    out.push(chunk.length, ...chunk);
  }
  out.push(0x00, 0x3b); // block terminator, trailer
  return new Uint8Array(out);
}

const path = join(import.meta.dir, "issue-6150.gif");
await Bun.write(path, makeGif());
console.log(`wrote ${path}`);
