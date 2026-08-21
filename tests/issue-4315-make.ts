// Generate tests/issue-4315.azw3: a KF8-style MOBI with empty PalmDoc HTML
// and two solid-color JPEG pages. EngineMobi must synthesize recindex pages
// so the book is not blank (issue #4315).
//
// Run: bun tests/issue-4315-make.ts

import { writeFileSync } from "node:fs";
import { join } from "node:path";

function be16(n: number): Buffer {
  const b = Buffer.alloc(2);
  b.writeUInt16BE(n);
  return b;
}
function be32(n: number): Buffer {
  const b = Buffer.alloc(4);
  b.writeUInt32BE(n);
  return b;
}

function jpegSolid(r: number, g: number, b: number): Buffer {
  // 8x8 baseline JPEG, 1 MCU, Y-only-ish via 1:1:1. Built as a tiny JFIF so
  // GuessFileTypeFromData recognizes it. Quality is irrelevant; we just need
  // two distinct decodable images of similar size.
  const ps = [
    "Add-Type -AssemblyName System.Drawing",
    `$bmp=New-Object System.Drawing.Bitmap 400,600`,
    `$gr=[System.Drawing.Graphics]::FromImage($bmp)`,
    `$gr.Clear([System.Drawing.Color]::FromArgb(${r},${g},${b}))`,
    `$gr.Dispose()`,
    `$ms=New-Object System.IO.MemoryStream`,
    `$bmp.Save($ms,[System.Drawing.Imaging.ImageFormat]::Jpeg)`,
    `$bmp.Dispose()`,
    `[Convert]::ToBase64String($ms.ToArray())`,
  ].join("; ");
  const out = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  const b64 = out.stdout.toString().trim();
  if (!b64) {
    throw new Error(`jpegSolid failed: ${out.stderr.toString()}`);
  }
  return Buffer.from(b64, "base64");
}

export function makeAzw3(): Buffer {
  const html = Buffer.from("<html><body></body></html>", "ascii");
  const imgA = jpegSolid(200, 30, 30);
  const imgB = jpegSolid(30, 30, 200);

  const mobi = Buffer.alloc(116);
  Buffer.from("MOBI").copy(mobi, 0);
  mobi.writeUInt32BE(116, 4);
  mobi.writeUInt32BE(2, 8); // book
  mobi.writeUInt32BE(65001, 12); // utf-8
  mobi.writeUInt32BE(1, 16);
  mobi.writeUInt32BE(8, 20); // KF8
  mobi.writeUInt32BE(2, 64); // firstNonBookRec
  mobi.writeUInt32BE(16 + 116, 68); // fullNameOffset in rec0
  mobi.writeUInt32BE(4, 72);
  mobi.writeUInt32BE(2, 92); // imageFirstRec

  const rec0 = Buffer.concat([
    be16(1),
    be16(0),
    be32(html.length),
    be16(1),
    be16(4096),
    be16(0),
    be16(0),
    mobi,
    Buffer.from("TEST"),
  ]);
  const recs = [rec0, html, imgA, imgB];
  const n = recs.length;
  const hdr = Buffer.alloc(78);
  Buffer.from("Test").copy(hdr, 0);
  Buffer.from("BOOKMOBI").copy(hdr, 60);
  hdr.writeUInt16BE(n, 76);
  let off = 78 + n * 8 + 2;
  const tableParts: Buffer[] = [];
  for (let i = 0; i < n; i++) {
    const row = Buffer.alloc(8);
    row.writeUInt32BE(off, 0);
    row[4] = 0;
    row[7] = i + 1;
    tableParts.push(row);
    off += recs[i]!.length;
  }
  return Buffer.concat([hdr, ...tableParts, Buffer.from([0, 0]), ...recs]);
}

if (import.meta.main) {
  const path = join(import.meta.dir, "issue-4315.azw3");
  writeFileSync(path, makeAzw3());
  console.log("wrote", path);
}
