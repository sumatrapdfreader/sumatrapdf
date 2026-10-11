// Minimal PNG codec for build scripts: 8-bit RGB/RGBA in, RGBA out.

import { deflateSync, inflateSync } from "node:zlib";

export type Rgba = { w: number; h: number; rgba: Buffer };

function paeth(a: number, b: number, c: number): number {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

export function decodePngRgba(png: Buffer): Rgba {
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
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([t, data])), 0);
  return Buffer.concat([len, t, data, crc]);
}

// filter None, RGBA
export function encodePngRgba(w: number, h: number, rgba: Buffer): Buffer {
  const sig = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 6; // RGBA
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    raw[y * (w * 4 + 1)] = 0;
    rgba.copy(raw, y * (w * 4 + 1) + 1, y * w * 4, (y + 1) * w * 4);
  }
  const idat = deflateSync(raw, { level: 9 });
  return Buffer.concat([sig, chunk("IHDR", ihdr), chunk("IDAT", idat), chunk("IEND", Buffer.alloc(0))]);
}

// box filter on premultiplied alpha; src must be a whole multiple of the target
export function downsampleRgba(src: Rgba, w: number, h: number): Rgba {
  const fx = src.w / w;
  const fy = src.h / h;
  if (!Number.isInteger(fx) || !Number.isInteger(fy)) {
    throw new Error(`${src.w}x${src.h} is not a multiple of ${w}x${h}`);
  }
  const out = Buffer.alloc(w * h * 4);
  const n = fx * fy;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let r = 0,
        g = 0,
        b = 0,
        a = 0;
      for (let sy = y * fy; sy < (y + 1) * fy; sy++) {
        for (let sx = x * fx; sx < (x + 1) * fx; sx++) {
          const si = (sy * src.w + sx) * 4;
          const sa = src.rgba[si + 3];
          r += src.rgba[si] * sa;
          g += src.rgba[si + 1] * sa;
          b += src.rgba[si + 2] * sa;
          a += sa;
        }
      }
      const di = (y * w + x) * 4;
      if (a === 0) continue;
      out[di] = Math.round(r / a);
      out[di + 1] = Math.round(g / a);
      out[di + 2] = Math.round(b / a);
      out[di + 3] = Math.round(a / n);
    }
  }
  return { w, h, rgba: out };
}
