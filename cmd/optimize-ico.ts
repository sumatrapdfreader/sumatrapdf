// Optimize .ico files, conservatively (no compatibility risk):
//   - drop the 128x128 frame: it's not a standard Windows icon-list size
//     (16/32/48/256 are), it's only a downscale source for the 96px "Large
//     icons" view, and it's stored as an uncompressed ~67 KB BMP. Windows
//     downscales the 256 PNG instead.
//   - re-compress existing PNG frames (the 256x256) with zopflipng (lossless).
//   - leave the 16/32/48/64 BMP frames untouched (real/near system sizes, and
//     small-size PNG-in-ICO isn't reliably supported everywhere).
//
// Usage: bun cmd/optimize-ico.ts <file.ico|dir> [...]   (defaults to gfx/gfxalt)

import { readFileSync, writeFileSync, readdirSync, statSync, mkdtempSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

const ZOPFLIPNG = join(import.meta.dir, "..", "bin", "zopflipng.exe");

function zopfli(png: Buffer, scratch: string): Buffer {
  const inP = join(scratch, "in.png"), outP = join(scratch, "out.png");
  writeFileSync(inP, png);
  const p = Bun.spawnSync({ cmd: [ZOPFLIPNG, "-y", inP, outP], stdout: "ignore", stderr: "ignore" });
  if (p.exitCode !== 0) return png;
  const out = readFileSync(outP);
  return out.length < png.length ? out : png;
}

type Frame = { w: number; h: number; bpp: number; data: Buffer };

function optimizeIco(path: string, scratch: string): boolean {
  const buf = readFileSync(path);
  if (buf.readUInt16LE(2) !== 1) return false; // not an icon
  const count = buf.readUInt16LE(4);
  const frames: Frame[] = [];
  const notes: string[] = [];
  for (let i = 0; i < count; i++) {
    const o = 6 + i * 16;
    const w = buf[o] || 256, h = buf[o + 1] || 256;
    const bpp = buf.readUInt16LE(o + 6);
    const size = buf.readUInt32LE(o + 8);
    const fo = buf.readUInt32LE(o + 12);
    const orig = buf.subarray(fo, fo + size);

    if (w === 128 && h === 128) {
      notes.push(`drop 128x128 (${size}b)`);
      continue;
    }
    let data = Buffer.from(orig);
    const isPng = orig[0] === 0x89 && orig[1] === 0x50;
    if (isPng) {
      const z = zopfli(Buffer.from(orig), scratch);
      if (z.length < data.length) {
        notes.push(`zopfli ${w}x${h} ${data.length}->${z.length}b`);
        data = z;
      }
    }
    frames.push({ w, h, bpp, data });
  }
  if (notes.length === 0) return false;

  // reassemble
  const n = frames.length;
  const head = Buffer.alloc(6 + n * 16);
  head.writeUInt16LE(0, 0); head.writeUInt16LE(1, 2); head.writeUInt16LE(n, 4);
  let dataOff = 6 + n * 16;
  const blobs: Buffer[] = [];
  frames.forEach((f, i) => {
    const o = 6 + i * 16;
    head[o] = f.w >= 256 ? 0 : f.w;
    head[o + 1] = f.h >= 256 ? 0 : f.h;
    head[o + 2] = 0; head[o + 3] = 0;
    head.writeUInt16LE(1, o + 4); // planes
    head.writeUInt16LE(f.bpp, o + 6);
    head.writeUInt32LE(f.data.length, o + 8);
    head.writeUInt32LE(dataOff, o + 12);
    dataOff += f.data.length;
    blobs.push(f.data);
  });
  const out = Buffer.concat([head, ...blobs]);
  if (out.length >= buf.length) return false;
  writeFileSync(path, out);
  const name = path.split(/[\\/]/).pop();
  console.log(`  ${name}: ${buf.length} -> ${out.length} (-${(((buf.length - out.length) / buf.length) * 100).toFixed(0)}%)  [${notes.join(", ")}]`);
  return true;
}

function main() {
  const args = process.argv.slice(2);
  const targets = args.length ? args : [join(import.meta.dir, "..", "gfx", "gfxalt")];
  const icos: string[] = [];
  for (const t of targets) {
    if (statSync(t).isDirectory()) {
      for (const f of readdirSync(t)) if (f.toLowerCase().endsWith(".ico")) icos.push(join(t, f));
    } else if (t.toLowerCase().endsWith(".ico")) icos.push(t);
  }
  const scratch = mkdtempSync(join(tmpdir(), "ico-"));
  let totalBefore = 0, totalAfter = 0, nChanged = 0;
  console.log(`optimizing ${icos.length} .ico files:`);
  for (const ico of icos) {
    const before = statSync(ico).size;
    const did = optimizeIco(ico, scratch);
    totalBefore += before;
    totalAfter += statSync(ico).size;
    if (did) nChanged++;
  }
  rmSync(scratch, { recursive: true, force: true });
  console.log(`\noptimized ${nChanged}/${icos.length} files: ${totalBefore} -> ${totalAfter} bytes (-${(((totalBefore - totalAfter) / totalBefore) * 100).toFixed(1)}%)`);
}

main();
