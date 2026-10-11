// Rasterize src/gfx/svg/*.svg to Windows .ico files under src/gfx/ or src/shared/gfx/:
// 256 PNG + 64/48/32/16 BMP (32bpp + AND mask). Optionally zopflipng-compresses
// the 256 PNG frame.
//
// Usage: bun cmd/svg-to-ico.ts
// Requires: bin/resvg.exe
//   Download: https://github.com/linebender/resvg/releases (resvg-win64.zip)
//   Optional: bin/zopflipng.exe for smaller 256 PNGs

import { readFileSync, writeFileSync, readdirSync, mkdtempSync, rmSync, existsSync, mkdirSync } from "node:fs";
import { join, basename } from "node:path";
import { tmpdir } from "node:os";
import { sharedSource } from "./helper/ng-shared";
import { decodePngRgba, encodePngRgba } from "./helper/png";

const ROOT = join(import.meta.dir, "..");
const RESVG = join(ROOT, "bin", "resvg.exe");
const ZOPFLIPNG = join(ROOT, "bin", "zopflipng.exe");
const SVG_DIR = join(ROOT, "src", "gfx", "svg");
const ICO_DIR = join(ROOT, "src", "gfx");
const SIZES = [256, 64, 48, 32, 16] as const;

function runResvg(svg: string, png: string, size: number): void {
  const p = Bun.spawnSync({
    cmd: [RESVG, "-w", String(size), "-h", String(size), svg, png],
    stdout: "inherit",
    stderr: "inherit",
  });
  if (p.exitCode !== 0) {
    throw new Error(`resvg failed for ${svg} @ ${size}`);
  }
}

// ICO DIB: BITMAPINFOHEADER + BGRA XOR bitmap (bottom-up) + 1bpp AND mask
function rgbaToIcoBmp(w: number, h: number, rgba: Buffer): Buffer {
  const rowXor = (w * 4 + 3) & ~3;
  const xorSize = rowXor * h;
  const rowAnd = ((w + 31) >> 5) * 4; // 1bpp, padded to 32-bit
  const andSize = rowAnd * h;
  const headerSize = 40;
  const buf = Buffer.alloc(headerSize + xorSize + andSize);
  // BITMAPINFOHEADER
  buf.writeUInt32LE(40, 0);
  buf.writeInt32LE(w, 4);
  buf.writeInt32LE(h * 2, 8); // XOR + AND
  buf.writeUInt16LE(1, 12); // planes
  buf.writeUInt16LE(32, 14); // bpp
  buf.writeUInt32LE(0, 16); // BI_RGB
  buf.writeUInt32LE(xorSize, 20);
  // XOR: bottom-up BGRA
  for (let y = 0; y < h; y++) {
    const srcY = h - 1 - y;
    for (let x = 0; x < w; x++) {
      const si = (srcY * w + x) * 4;
      const di = headerSize + y * rowXor + x * 4;
      buf[di] = rgba[si + 2]; // B
      buf[di + 1] = rgba[si + 1]; // G
      buf[di + 2] = rgba[si]; // R
      buf[di + 3] = rgba[si + 3]; // A
    }
  }
  // AND mask: 0 for all (alpha in XOR is used on modern Windows)
  // leave zeros
  return buf;
}

function zopfliPng(png: Buffer, scratch: string): Buffer {
  if (!existsSync(ZOPFLIPNG)) return png;
  const inP = join(scratch, "in.png");
  const outP = join(scratch, "out.png");
  writeFileSync(inP, png);
  const p = Bun.spawnSync({
    cmd: [ZOPFLIPNG, "-y", inP, outP],
    stdout: "ignore",
    stderr: "ignore",
  });
  if (p.exitCode !== 0 || !existsSync(outP)) return png;
  const out = readFileSync(outP);
  return out.length < png.length ? out : png;
}

function buildIco(frames: { w: number; h: number; bpp: number; data: Buffer }[]): Buffer {
  const n = frames.length;
  const head = Buffer.alloc(6 + n * 16);
  head.writeUInt16LE(0, 0);
  head.writeUInt16LE(1, 2);
  head.writeUInt16LE(n, 4);
  let dataOff = 6 + n * 16;
  const blobs: Buffer[] = [];
  frames.forEach((f, i) => {
    const o = 6 + i * 16;
    head[o] = f.w >= 256 ? 0 : f.w;
    head[o + 1] = f.h >= 256 ? 0 : f.h;
    head[o + 2] = 0;
    head[o + 3] = 0;
    head.writeUInt16LE(1, o + 4);
    head.writeUInt16LE(f.bpp, o + 6);
    head.writeUInt32LE(f.data.length, o + 8);
    head.writeUInt32LE(dataOff, o + 12);
    dataOff += f.data.length;
    blobs.push(f.data);
  });
  return Buffer.concat([head, ...blobs]);
}

function svgToIco(svgPath: string, icoPath: string, scratch: string): void {
  const frames: { w: number; h: number; bpp: number; data: Buffer }[] = [];
  for (const size of SIZES) {
    const pngPath = join(scratch, `${size}.png`);
    runResvg(svgPath, pngPath, size);
    const png = readFileSync(pngPath);
    const { w, h, rgba } = decodePngRgba(png);
    if (w !== size || h !== size) {
      throw new Error(`size mismatch ${w}x${h} expected ${size}`);
    }
    if (size === 256) {
      // re-encode + zopfli so 256 is compact PNG-in-ICO
      let outPng = encodePngRgba(w, h, rgba);
      outPng = zopfliPng(outPng, scratch);
      // if zopfli of re-encode is larger than resvg's PNG, keep the smaller
      const best = outPng.length < png.length ? outPng : zopfliPng(png, scratch);
      frames.push({ w, h, bpp: 32, data: best });
    } else {
      frames.push({ w, h, bpp: 32, data: rgbaToIcoBmp(w, h, rgba) });
    }
  }
  // order: 256, 64, 48, 32, 16
  writeFileSync(icoPath, buildIco(frames));
}

function main() {
  if (!existsSync(RESVG)) {
    console.error(`missing ${RESVG}`);
    process.exit(1);
  }
  const svgs = readdirSync(SVG_DIR).filter((f) => f.endsWith(".svg"));
  if (svgs.length === 0) {
    console.error(`no SVGs in ${SVG_DIR}`);
    process.exit(1);
  }
  const scratchRoot = mkdtempSync(join(tmpdir(), "svg-ico-"));
  console.log(`converting ${svgs.length} SVGs in ${SVG_DIR} -> ${ICO_DIR}`);
  try {
    for (const name of svgs) {
      const svg = join(SVG_DIR, name);
      const ico = join(ROOT, sharedSource("src/gfx/" + name.replace(/\.svg$/i, ".ico")));
      const scratch = join(scratchRoot, name);
      mkdirSync(scratch, { recursive: true });
      svgToIco(svg, ico, scratch);
      const kb = (readFileSync(ico).length / 1024).toFixed(1);
      console.log(`  ${basename(ico)}  ${kb} KB`);
    }
  } finally {
    rmSync(scratchRoot, { recursive: true, force: true });
  }
  console.log("done");
}

main();
