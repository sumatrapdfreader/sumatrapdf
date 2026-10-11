// Regenerates src/gfx/mac/SumatraPDF.icns from src/gfx/appx/fileicon.png,
// the 256 px image of the Windows app icon, so both platforms share one
// artwork (issue #6317). Pure TypeScript: no sips / iconutil needed.
//
// Usage: bun cmd/update-mac-icon.ts

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { decodePngRgba, downsampleRgba, encodePngRgba, type Rgba } from "./helper/png";

const ROOT = join(import.meta.dir, "..");
const SRC_PNG = join(ROOT, "src", "gfx", "appx", "fileicon.png");
const DST_ICNS = join(ROOT, "src", "gfx", "mac", "SumatraPDF.icns");

// .icns element types holding a PNG, by pixel size. 1x and 2x entries of the
// same pixel size carry the same image. The source is 256 px, so the 512 and
// 1024 px types (ic09, ic14, ic10) are left out: macOS scales ic08 up instead.
const ICNS_TYPES: { type: string; px: number }[] = [
  { type: "icp4", px: 16 },
  { type: "icp5", px: 32 },
  { type: "icp6", px: 64 },
  { type: "ic07", px: 128 },
  { type: "ic08", px: 256 },
  { type: "ic11", px: 32 }, // 16@2x
  { type: "ic12", px: 64 }, // 32@2x
  { type: "ic13", px: 256 }, // 128@2x
];

const HEADER_SIZE = 8; // 4-byte type + 4-byte big-endian length, for file and elements

function element(type: string, data: Buffer): Buffer {
  const head = Buffer.alloc(HEADER_SIZE);
  head.write(type, 0, "ascii");
  head.writeUInt32BE(HEADER_SIZE + data.length, 4);
  return Buffer.concat([head, data]);
}

// the source file is used as-is at its own size (it is already well compressed)
function buildIcns(srcPng: Buffer, src: Rgba): Buffer {
  const pngBySize = new Map<number, Buffer>([[src.w, srcPng]]);
  const pngFor = (px: number): Buffer => {
    let png = pngBySize.get(px);
    if (!png) {
      const img = downsampleRgba(src, px, px);
      png = encodePngRgba(img.w, img.h, img.rgba);
      pngBySize.set(px, png);
    }
    return png;
  };
  const elements = ICNS_TYPES.map((t) => element(t.type, pngFor(t.px)));
  return element("icns", Buffer.concat(elements));
}

function main(): void {
  const srcPng = readFileSync(SRC_PNG);
  const src = decodePngRgba(srcPng);
  if (src.w !== src.h) throw new Error(`${SRC_PNG} is ${src.w}x${src.h}, expected square`);
  const icns = buildIcns(srcPng, src);
  mkdirSync(join(DST_ICNS, ".."), { recursive: true });
  writeFileSync(DST_ICNS, icns);
  const types = ICNS_TYPES.map((t) => `${t.type}=${t.px}`).join(" ");
  console.log(`wrote ${DST_ICNS} (${icns.length} bytes): ${types}`);
}

main();
