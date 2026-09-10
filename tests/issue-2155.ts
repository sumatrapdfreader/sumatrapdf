// #2155: SVGs that carry their colors in a <style> sheet (class="st0") rendered
// all black, because mupdf's SVG parser only looked at presentation attributes
// and the inline style attribute.
//
// issue-2155.svg is the reported file with its one <text> element removed, so
// the test needs no fonts (the copied test exe cannot reach the embedded ones).
//
// Run: bun tests/issue-2155.ts [--no-build]

import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { inflateSync } from "node:zlib";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

type Image = { w: number; h: number; data: Uint8Array };

function paeth(a: number, b: number, c: number): number {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) {
    return a;
  }
  if (pb <= pc) {
    return b;
  }
  return c;
}

// Minimal PNG reader for what `draw` emits: 8-bit RGB or RGBA, no interlace.
function loadPng(path: string): Image {
  const buf = readFileSync(path);
  let off = 8;
  let w = 0;
  let h = 0;
  let colorType = 0;
  const idat: Buffer[] = [];

  while (off + 8 <= buf.length) {
    const n = buf.readUInt32BE(off);
    const type = buf.toString("latin1", off + 4, off + 8);
    const data = buf.subarray(off + 8, off + 8 + n);
    if (type === "IHDR") {
      w = data.readUInt32BE(0);
      h = data.readUInt32BE(4);
      colorType = data[9]!;
    } else if (type === "IDAT") {
      idat.push(data);
    } else if (type === "IEND") {
      break;
    }
    off += 12 + n;
  }

  const nComp = colorType === 2 ? 3 : 4;
  if ((colorType !== 2 && colorType !== 6) || w <= 0 || h <= 0) {
    throw new Error(`issue-2155: unsupported png ${path} ct=${colorType} ${w}x${h}`);
  }

  const raw = inflateSync(Buffer.concat(idat));
  const stride = w * nComp;
  const data = new Uint8Array(h * stride);
  let src = 0;
  for (let y = 0; y < h; y++) {
    const filter = raw[src++]!;
    const row = y * stride;
    const prev = (y - 1) * stride;
    for (let x = 0; x < stride; x++) {
      const left = x >= nComp ? data[row + x - nComp]! : 0;
      const up = y > 0 ? data[prev + x]! : 0;
      const ul = y > 0 && x >= nComp ? data[prev + x - nComp]! : 0;
      const v = raw[src++]!;
      let recon = v;
      if (filter === 1) {
        recon = (v + left) & 255;
      } else if (filter === 2) {
        recon = (v + up) & 255;
      } else if (filter === 3) {
        recon = (v + ((left + up) >> 1)) & 255;
      } else if (filter === 4) {
        recon = (v + paeth(left, up, ul)) & 255;
      } else if (filter !== 0) {
        throw new Error(`issue-2155: png filter ${filter}`);
      }
      data[row + x] = recon;
    }
  }

  return { w, h, data: nComp === 3 ? toRgba(w, h, data) : data };
}

function toRgba(w: number, h: number, rgb: Uint8Array): Uint8Array {
  const out = new Uint8Array(w * h * 4);
  for (let i = 0, j = 0; i < w * h; i++, j += 3) {
    out[i * 4] = rgb[j]!;
    out[i * 4 + 1] = rgb[j + 1]!;
    out[i * 4 + 2] = rgb[j + 2]!;
    out[i * 4 + 3] = 255;
  }
  return out;
}

const BLACK_MAX = 40; // per-channel value below which we call a pixel black
const WHITE_MIN = 240;

function isBlack(img: Image, i: number): boolean {
  return img.data[i]! < BLACK_MAX && img.data[i + 1]! < BLACK_MAX && img.data[i + 2]! < BLACK_MAX;
}

function isWhite(img: Image, i: number): boolean {
  return img.data[i]! > WHITE_MIN && img.data[i + 1]! > WHITE_MIN && img.data[i + 2]! > WHITE_MIN;
}

export async function testit(): Promise<void> {
  const svg = join(ROOT, "tests", "issue-2155.svg");
  const png = tmpPath("issue-2155.png");

  const res = spawnSync(EXE, ["draw", "-o", png, "-r", "18", svg], { encoding: "utf8" });
  if (res.status !== 0) {
    throw new Error(`issue-2155: draw failed (${res.status}): ${res.stderr || res.stdout}`);
  }

  const img = loadPng(png);
  let black = 0;
  let white = 0;
  for (let i = 0; i < img.w * img.h * 4; i += 4) {
    if (isBlack(img, i)) {
      black++;
    } else if (isWhite(img, i)) {
      white++;
    }
  }

  // .st0 fills the whole 1000x1000 canvas white, so most of the page is white
  // and the black ink is a small minority.
  const total = img.w * img.h;
  if (!isWhite(img, 0)) {
    throw new Error(`issue-2155: top-left pixel is not white, <style> classes ignored (${png})`);
  }
  if (black > total / 4) {
    throw new Error(`issue-2155: ${black}/${total} pixels are black, expected mostly white (${png})`);
  }
  if (white < total / 2) {
    throw new Error(`issue-2155: only ${white}/${total} pixels are white (${png})`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
