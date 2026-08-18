// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3434
//
// Opening a small PNG and copying a selection used to shift the image by half
// a pixel (and enlarge the dest by 1px). CvtFromScreen pulls a pixel-aligned
// selection back by 0.499px, and GDI+'s HighQuality pixel-offset mode then
// sampled from that float. Copy Image (the raw pixels) was fine.
//
// The fixture is 32x32: 4 green columns on the left, 4 blue on the right, red
// in the middle. A full-page render and a Copy-Selection-style clip (page rect
// at -0.499) must both come back 32x32 with a green left edge and a blue right
// edge — not 33x33, and not a red or mixed edge. Image → Resize uses the same
// GDI+ draw, so 1:1 / 2x / half-size copies must keep those edges too.
//
// Run:  bun tests/issue-3434.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const W = 32;
const H = 32;
const EDGE = 4;

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

function makeEdgePng(): Buffer {
  const raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) {
    const row = y * (W * 3 + 1);
    raw[row] = 0;
    for (let x = 0; x < W; x++) {
      const i = row + 1 + x * 3;
      if (x < EDGE) {
        raw[i] = 0;
        raw[i + 1] = 220;
        raw[i + 2] = 0;
      } else if (x >= W - EDGE) {
        raw[i] = 0;
        raw[i + 1] = 0;
        raw[i + 2] = 220;
      } else {
        raw[i] = 220;
        raw[i + 1] = 0;
        raw[i + 2] = 0;
      }
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0);
  ihdr.writeUInt32BE(H, 4);
  ihdr[8] = 8;
  ihdr[9] = 2; // truecolor
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function parseEdges(raw: string): { w: number; h: number; left: number[]; right: number[] } {
  const m = /size=(\d+)x(\d+) left=(\d+),(\d+),(\d+) right=(\d+),(\d+),(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-3434: could not parse: ${raw}`);
  }
  return {
    w: +m[1]!,
    h: +m[2]!,
    left: [+m[3]!, +m[4]!, +m[5]!],
    right: [+m[6]!, +m[7]!, +m[8]!],
  };
}

function isGreen(c: number[]): boolean {
  return c[1]! > 150 && c[0]! < 80 && c[2]! < 80;
}

function isBlue(c: number[]): boolean {
  return c[2]! > 150 && c[0]! < 80 && c[1]! < 80;
}

export async function testit(): Promise<void> {
  const png = tmpPath("issue-3434.png");
  writeFileSync(png, makeEdgePng());

  await withControlledSumatra(EXE, async (client) => {
    for (const [label, zoom, clipKind] of [
      ["full page 100%", 100, 0],
      ["copy-selection clip 100%", 100, 1],
      ["copy-selection clip 200%", 200, 1],
    ] as const) {
      const res = await client.request(ControlCommand.TestImageRenderEdges, [png, zoom, clipKind]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-3434 ${label}: ${raw.trim()}`);
      }
      const e = parseEdges(raw);
      const want = (W * zoom) / 100;
      if (e.w !== want || e.h !== want) {
        throw new Error(`issue-3434 ${label}: dest ${e.w}x${e.h}, want ${want}x${want}:\n${raw}`);
      }
      if (!isGreen(e.left)) {
        throw new Error(`issue-3434 ${label}: left edge ${e.left.join(",")} is not green (shifted?):\n${raw}`);
      }
      if (!isBlue(e.right)) {
        throw new Error(`issue-3434 ${label}: right edge ${e.right.join(",")} is not blue (shifted?):\n${raw}`);
      }
      console.log(`  ${label}: ${e.w}x${e.h} left green right blue ✓`);
    }

    // Image → Resize used the same GDI+ HighQuality pixel-offset, so a
    // scaled copy was still shifted (issue #3434 comment).
    for (const [label, newW, newH] of [
      ["resize 1:1", 32, 32],
      ["resize 2x", 64, 64],
      ["resize 1/2", 16, 16],
    ] as const) {
      const res = await client.request(ControlCommand.TestImageResizeEdges, [png, newW, newH]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-3434 ${label}: ${raw.trim()}`);
      }
      const e = parseEdges(raw);
      if (e.w !== newW || e.h !== newH) {
        throw new Error(`issue-3434 ${label}: dest ${e.w}x${e.h}, want ${newW}x${newH}:\n${raw}`);
      }
      if (!isGreen(e.left)) {
        throw new Error(`issue-3434 ${label}: left edge ${e.left.join(",")} is not green (shifted?):\n${raw}`);
      }
      if (!isBlue(e.right)) {
        throw new Error(`issue-3434 ${label}: right edge ${e.right.join(",")} is not blue (shifted?):\n${raw}`);
      }
      console.log(`  ${label}: ${e.w}x${e.h} left green right blue ✓`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
