// Rasterize gfx/svg/*.svg to multi-size Windows .ico files under gfx/:
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
import { deflateSync, inflateSync } from "node:zlib";

const ROOT = join(import.meta.dir, "..");
const RESVG = join(ROOT, "bin", "resvg.exe");
const ZOPFLIPNG = join(ROOT, "bin", "zopflipng.exe");
const SVG_DIR = join(ROOT, "gfx", "svg");
const ICO_DIR = join(ROOT, "gfx");
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

function paeth(a: number, b: number, c: number): number {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

function decodePngRgba(png: Buffer): { w: number; h: number; rgba: Buffer } {
  if (png[0] !== 0x89 || png[1] !== 0x50) throw new Error("not a PNG");
  let w = 0,
    h = 0,
    bitDepth = 0,
    colorType = 0;
  const idats: Buffer[] = [];
  let o = 8;
  while (o + 8 <= png.length) {
    const len = png.readUInt32BE(o);
    const type = png.toString("ascii", o + 4, o + 8);
    const data = png.subarray(o + 8, o + 8 + len);
    if (type === "IHDR") {
      w = data.readUInt32BE(0);
      h = data.readUInt32BE(4);
      bitDepth = data[8];
      colorType = data[9];
    } else if (type === "IDAT") idats.push(Buffer.from(data));
    else if (type === "IEND") break;
    o += 12 + len;
  }
  if (bitDepth !== 8 || (colorType !== 6 && colorType !== 2)) {
    throw new Error(`unsupported PNG: bitDepth=${bitDepth} colorType=${colorType}`);
  }
  const raw = inflateSync(Buffer.concat(idats));
  const srcBpp = colorType === 6 ? 4 : 3;
  const stride = 1 + w * srcBpp;
  const rgba = Buffer.alloc(w * h * 4);
  let prevRow: Buffer | null = null;
  for (let y = 0; y < h; y++) {
    const filter = raw[y * stride];
    const rowIn = raw.subarray(y * stride + 1, y * stride + stride);
    const cur = Buffer.alloc(w * srcBpp);
    for (let i = 0; i < w * srcBpp; i++) {
      const x = rowIn[i];
      const a = i >= srcBpp ? cur[i - srcBpp] : 0;
      const b = prevRow ? prevRow[i] : 0;
      const c = prevRow && i >= srcBpp ? prevRow[i - srcBpp] : 0;
      let v: number;
      switch (filter) {
        case 0:
          v = x;
          break;
        case 1:
          v = (x + a) & 0xff;
          break;
        case 2:
          v = (x + b) & 0xff;
          break;
        case 3:
          v = (x + ((a + b) >> 1)) & 0xff;
          break;
        case 4:
          v = (x + paeth(a, b, c)) & 0xff;
          break;
        default:
          throw new Error(`unknown PNG filter ${filter}`);
      }
      cur[i] = v;
    }
    for (let x = 0; x < w; x++) {
      const si = x * srcBpp;
      const di = (y * w + x) * 4;
      rgba[di] = cur[si];
      rgba[di + 1] = cur[si + 1];
      rgba[di + 2] = cur[si + 2];
      rgba[di + 3] = srcBpp === 4 ? cur[si + 3] : 255;
    }
    prevRow = cur;
  }
  return { w, h, rgba };
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

function encodePngRgba(w: number, h: number, rgba: Buffer): Buffer {
  // minimal PNG encoder, filter 0, RGBA
  function crc32(buf: Buffer): number {
    let c = ~0;
    for (let i = 0; i < buf.length; i++) {
      c ^= buf[i];
      for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    return ~c >>> 0;
  }
  function chunk(type: string, data: Buffer): Buffer {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length, 0);
    const t = Buffer.from(type, "ascii");
    const crcBuf = Buffer.concat([t, data]);
    const crc = Buffer.alloc(4);
    crc.writeUInt32BE(crc32(crcBuf), 0);
    return Buffer.concat([len, t, data, crc]);
  }
  const sig = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 6; // RGBA
  ihdr[10] = 0;
  ihdr[11] = 0;
  ihdr[12] = 0;
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (w * 4 + 1)] = 0; // filter None
    rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4);
  }
  const idat = deflateSync(raw, { level: 9 });
  return Buffer.concat([sig, chunk("IHDR", ihdr), chunk("IDAT", idat), chunk("IEND", Buffer.alloc(0))]);
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
      const ico = join(ICO_DIR, name.replace(/\.svg$/i, ".ico"));
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
