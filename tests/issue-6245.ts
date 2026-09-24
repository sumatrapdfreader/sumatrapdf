// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6245
//
// Comics with JPEG XL pages were slow to page through and zoom.
//
// 1. RenderCache asks for a tile as page -> pixels -> page. RenderPage snapped
//    that page rect to whole image pixels and rounded outward, so at about half
//    of all zoom levels the render came out 1px bigger than the tile. That
//    missed the fast mupdf path and decoded the page a second time. A
//    single-tile render must come back at exactly the tile's size.
// 2. JXL and WebP pages now decode straight into an fz_image (mupdf render
//    path) instead of a BGRA Pixmap scaled by GDI+. Colors must match the old
//    decode; EXIF-rotated WebP still goes through the Pixmap path.
//
// issue-6245-data/keong_macan.jxl is 64px_cvo9xd_keong_macan_srgb8.v_d1.jxl
// from the jxldec corpus (https://github.com/kjk/jxldec, deps/corpus/gen).
// The .webp files come from issue-6245-data/make-webp.ts.
//
// Run: bun tests/issue-6245.ts [--no-build]

import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { deflateSync } from "node:zlib";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const DATA = join(dirname(fileURLToPath(import.meta.url)), "issue-6245-data");

// odd size + zooms where the round trip used to grow the render by 1px
const PNG_W = 997;
const PNG_H = 1409;
const PNG_RGB = [40, 90, 200];
const MISMATCH_ZOOMS = [19, 41, 57];

const kClipNone = 0;
const kClipFullPageTile = 3;

type Edges = { file: string; zoom: number; w: number; h: number; left?: number[]; right?: number[] };

// size and edge colors the old (Pixmap) path rendered
const EDGES: Edges[] = [
  { file: "keong_macan.jxl", zoom: 100, w: 64, h: 64, left: [91, 128, 170], right: [6, 12, 19] },
  { file: "keong_macan.jxl", zoom: 50, w: 32, h: 32, left: [93, 129, 170], right: [6, 12, 17] },
  { file: "halves.webp", zoom: 100, w: 64, h: 64, left: [39, 90, 200], right: [231, 120, 30] },
  { file: "halves.webp", zoom: 50, w: 32, h: 32, left: [39, 90, 200], right: [231, 120, 30] },
  // right half is 50% transparent, composited on white
  { file: "halves-alpha.webp", zoom: 100, w: 64, h: 64, left: [40, 90, 200], right: [241, 186, 141] },
  { file: "halves-alpha.webp", zoom: 50, w: 32, h: 32, left: [40, 90, 200], right: [241, 186, 141] },
  // stored 64x32, EXIF orientation 6
  { file: "halves-rot90.webp", zoom: 100, w: 32, h: 64 },
];
const COLOR_TOLERANCE = 6;

function crc32(buf: Buffer): number {
  let crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    let c = (crc ^ buf[n]!) & 0xff;
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

function makeSolidPng(w: number, h: number, rgb: number[]): Buffer {
  const rowLen = w * 3 + 1;
  const raw = Buffer.alloc(rowLen * h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      raw.set(rgb, y * rowLen + 1 + x * 3);
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2; // truecolor
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

type Render = { w: number; h: number; left: number[]; right: number[]; tileW: number; tileH: number };

async function render(client: ControlClient, path: string, zoom: number, clipKind: number): Promise<Render> {
  const res = await client.request(ControlCommand.TestImageRenderEdges, [path, zoom, clipKind]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6245 ${zoom}%: ${raw}`);
  }
  const m = /size=(\d+)x(\d+) left=(\d+),(\d+),(\d+) right=(\d+),(\d+),(\d+)(?: tile=(\d+)x(\d+))?/.exec(raw);
  if (!m) {
    throw new Error(`issue-6245: could not parse: ${raw}`);
  }
  return {
    w: +m[1]!,
    h: +m[2]!,
    left: [+m[3]!, +m[4]!, +m[5]!],
    right: [+m[6]!, +m[7]!, +m[8]!],
    tileW: m[9] ? +m[9] : 0,
    tileH: m[10] ? +m[10] : 0,
  };
}

function near(got: number[], want: number[]): boolean {
  return got.every((v, i) => Math.abs(v - want[i]!) <= COLOR_TOLERANCE);
}

export async function testit(): Promise<void> {
  const png = tmpPath("issue-6245.png");
  writeFileSync(png, makeSolidPng(PNG_W, PNG_H, PNG_RGB));

  await withControlledSumatra(EXE, async (client) => {
    for (const zoom of MISMATCH_ZOOMS) {
      const r = await render(client, png, zoom, kClipFullPageTile);
      if (r.w !== r.tileW || r.h !== r.tileH) {
        throw new Error(`issue-6245 ${zoom}%: rendered ${r.w}x${r.h}, tile is ${r.tileW}x${r.tileH}`);
      }
      if (!near(r.left, PNG_RGB) || !near(r.right, PNG_RGB)) {
        throw new Error(`issue-6245 ${zoom}%: edges ${r.left} / ${r.right}, want ${PNG_RGB}`);
      }
      console.log(`  png ${zoom}%: ${r.w}x${r.h} matches tile ✓`);
    }

    for (const want of EDGES) {
      const r = await render(client, join(DATA, want.file), want.zoom, kClipNone);
      const name = `${want.file} ${want.zoom}%`;
      if (r.w !== want.w || r.h !== want.h) {
        throw new Error(`issue-6245 ${name}: rendered ${r.w}x${r.h}, want ${want.w}x${want.h}`);
      }
      if (want.left && want.right && (!near(r.left, want.left) || !near(r.right, want.right))) {
        throw new Error(
          `issue-6245 ${name}: edges ${r.left} / ${r.right}, want ${want.left} / ${want.right} (±${COLOR_TOLERANCE})`,
        );
      }
      console.log(`  ${name}: ${r.w}x${r.h} edges ${r.left} / ${r.right} ✓`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
