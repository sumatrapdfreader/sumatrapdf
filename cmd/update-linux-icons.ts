// Regenerates src/ng/linux/icons/sumatrapdf-<px>.png, the hicolor icon set
// of the Linux package, from src/gfx/appx/fileicon.png, the same artwork
// cmd/update-mac-icon.ts turns into the mac icon. Pure TypeScript.
//
// Usage: bun cmd/update-linux-icons.ts

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { decodePngRgba, encodePngRgba, type Rgba } from "./helper/png";

const ROOT = join(import.meta.dir, "..");
const SRC_PNG = join(ROOT, "src", "gfx", "appx", "fileicon.png");
const DST_DIR = join(ROOT, "src", "ng", "linux", "icons");

// the hicolor sizes desktops look up; 256 is the source itself
export const LINUX_ICON_SIZES = [16, 24, 32, 48, 64, 128, 256];

export function linuxIconName(px: number): string {
  return `sumatrapdf-${px}.png`;
}

// area average on premultiplied alpha; unlike png.ts downsampleRgba the
// factor needn't be whole (256 -> 48)
function resizeRgba(src: Rgba, w: number, h: number): Rgba {
  const out = Buffer.alloc(w * h * 4);
  const fx = src.w / w;
  const fy = src.h / h;
  for (let y = 0; y < h; y++) {
    const y0 = y * fy;
    const y1 = y0 + fy;
    for (let x = 0; x < w; x++) {
      const x0 = x * fx;
      const x1 = x0 + fx;
      let r = 0,
        g = 0,
        b = 0,
        a = 0;
      for (let sy = Math.floor(y0); sy < Math.ceil(y1); sy++) {
        const wy = Math.min(sy + 1, y1) - Math.max(sy, y0);
        for (let sx = Math.floor(x0); sx < Math.ceil(x1); sx++) {
          const wgt = wy * (Math.min(sx + 1, x1) - Math.max(sx, x0));
          const i = (sy * src.w + sx) * 4;
          const sa = src.rgba[i + 3]! * wgt;
          r += src.rgba[i]! * sa;
          g += src.rgba[i + 1]! * sa;
          b += src.rgba[i + 2]! * sa;
          a += sa;
        }
      }
      const o = (y * w + x) * 4;
      out[o] = a > 0 ? Math.round(r / a) : 0;
      out[o + 1] = a > 0 ? Math.round(g / a) : 0;
      out[o + 2] = a > 0 ? Math.round(b / a) : 0;
      out[o + 3] = Math.round(a / (fx * fy));
    }
  }
  return { w, h, rgba: out };
}

function main(): void {
  const srcPng = readFileSync(SRC_PNG);
  const src = decodePngRgba(srcPng);
  if (src.w !== src.h) throw new Error(`${SRC_PNG} is ${src.w}x${src.h}, expected square`);
  mkdirSync(DST_DIR, { recursive: true });
  for (const px of LINUX_ICON_SIZES) {
    let png: Buffer = srcPng;
    if (px !== src.w) {
      const img = resizeRgba(src, px, px);
      png = encodePngRgba(img.w, img.h, img.rgba);
    }
    const dst = join(DST_DIR, linuxIconName(px));
    writeFileSync(dst, png);
    console.log(`${dst}: ${png.length} bytes`);
  }
}

if (import.meta.main) main();
