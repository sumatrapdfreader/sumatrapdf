// Open a multi-size .ico: each size is a page (WIC ICO decoder).
//
// Run: bun tests/ico.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function icoDib(w: number, h: number, b: number, g: number, r: number): Buffer {
  const xorStride = w * 4;
  const andStride = ((w + 31) >> 5) * 4;
  const xorSize = xorStride * h;
  const andSize = andStride * h;
  const buf = Buffer.alloc(40 + xorSize + andSize);
  buf.writeUInt32LE(40, 0); // biSize
  buf.writeInt32LE(w, 4);
  buf.writeInt32LE(h * 2, 8); // XOR + AND
  buf.writeUInt16LE(1, 12); // planes
  buf.writeUInt16LE(32, 14); // bitcount
  for (let y = 0; y < h; y++) {
    const row = 40 + (h - 1 - y) * xorStride;
    for (let x = 0; x < w; x++) {
      const i = row + x * 4;
      buf[i] = b;
      buf[i + 1] = g;
      buf[i + 2] = r;
      buf[i + 3] = 255;
    }
  }
  return buf;
}

function makeIco(images: { w: number; h: number; b: number; g: number; r: number }[]): Buffer {
  const dibs = images.map((im) => icoDib(im.w, im.h, im.b, im.g, im.r));
  const dir = Buffer.alloc(6 + 16 * images.length);
  dir.writeUInt16LE(1, 2); // type = ICO
  dir.writeUInt16LE(images.length, 4);
  let off = 6 + 16 * images.length;
  for (let i = 0; i < images.length; i++) {
    const e = 6 + i * 16;
    const im = images[i]!;
    dir[e] = im.w >= 256 ? 0 : im.w;
    dir[e + 1] = im.h >= 256 ? 0 : im.h;
    dir.writeUInt16LE(1, e + 4); // planes
    dir.writeUInt16LE(32, e + 6); // bitcount
    dir.writeUInt32LE(dibs[i]!.length, e + 8);
    dir.writeUInt32LE(off, e + 12);
    off += dibs[i]!.length;
  }
  return Buffer.concat([dir, ...dibs]);
}

export async function testit(): Promise<void> {
  const icoPath = tmpPath("ico-two-size.ico");
  writeFileSync(
    icoPath,
    makeIco([
      { w: 16, h: 16, b: 0, g: 0, r: 200 },
      { w: 32, h: 32, b: 200, g: 0, r: 0 },
    ]),
  );

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestLayout, ["get"]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`ico: TestLayout failed: ${raw.trim()}`);
      }
      const m = /pages count=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`ico: no pages count in layout (did the .ico fail to open?):\n${raw}`);
      }
      const n = +m[1]!;
      if (n !== 2) {
        throw new Error(`ico: pages count=${n}, want 2 (one page per size):\n${raw}`);
      }
      console.log("  opened 16x16 + 32x32 .ico as 2 pages");
    },
    [icoPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
