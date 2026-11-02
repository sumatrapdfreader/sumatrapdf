// #6470: a comic's JPEG stored portrait with EXIF orientation 6 ("rotate 90°
// clockwise") showed unrotated in its landscape page box, the rest of the box
// filled with a mirrored copy. The fallback that decodes such an image through
// MuPDF didn't apply the orientation, which MuPDF leaves to the caller.
//
// The JPEG is red on its left quarter and blue on the rest; rotated clockwise
// the red is on top.
//
// Run: bun tests/issue-6470.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { makeStoredZip, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowPixels } from "./winapi.ts";

const EXIF_ROTATE_CW = 6;

// a minimal little-endian EXIF block with only the Orientation tag
function exifApp1(orientation: number): Buffer {
  const tiff = Buffer.alloc(8 + 2 + 12 + 4);
  tiff.write("II", 0, "latin1");
  tiff.writeUInt16LE(42, 2);
  tiff.writeUInt32LE(8, 4); // first IFD
  tiff.writeUInt16LE(1, 8); // one entry
  tiff.writeUInt16LE(0x0112, 10); // Orientation
  tiff.writeUInt16LE(3, 12); // SHORT
  tiff.writeUInt32LE(1, 14);
  tiff.writeUInt16LE(orientation, 18);
  tiff.writeUInt32LE(0, 22); // no next IFD
  const payload = Buffer.concat([Buffer.from("Exif\0\0", "latin1"), tiff]);
  const marker = Buffer.from([0xff, 0xe1, 0, 0]);
  marker.writeUInt16BE(payload.length + 2, 2);
  return Buffer.concat([marker, payload]);
}

// the JPEG with an EXIF block right after its SOI
function withOrientation(jpeg: Buffer, orientation: number): Buffer {
  return Buffer.concat([jpeg.subarray(0, 2), exifApp1(orientation), jpeg.subarray(2)]);
}

type Rgb = [number, number, number];
const isRed = (c: Rgb) => c[0] > 200 && c[1] < 80 && c[2] < 80;
const isBlue = (c: Rgb) => c[2] > 200 && c[0] < 80 && c[1] < 80;

export async function testit(): Promise<void> {
  const jpeg = readFileSync(join(import.meta.dir, "issue-6229.jpg"));
  const cbz = tmpPath("issue-6470.cbz");
  writeFileSync(cbz, makeStoredZip([{ name: "page.jpg", data: withOrientation(jpeg, EXIF_ROTATE_CW) }]));

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", cbz]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const cap = captureWindowPixels(findCanvas(frame));
    if (!cap) {
      throw new Error("issue-6470: canvas capture failed");
    }
    const px = (x: number, y: number): Rgb => {
      const i = (y * cap.w + x) * 4;
      return [cap.data[i + 2]!, cap.data[i + 1]!, cap.data[i]!];
    };
    const isPage = (c: Rgb) => isRed(c) || isBlue(c);

    // the page's colors at its top and on its left, down the middle column and across the middle row
    const midX = Math.floor(cap.w / 2);
    const midY = Math.floor(cap.h / 2);
    let top: Rgb | null = null;
    for (let y = 0; y < cap.h && !top; y++) {
      if (isPage(px(midX, y))) {
        top = px(midX, y);
      }
    }
    let left: Rgb | null = null;
    for (let x = 0; x < cap.w && !left; x++) {
      if (isPage(px(x, midY))) {
        left = px(x, midY);
      }
    }
    if (!top || !left) {
      throw new Error("issue-6470: the page didn't render");
    }
    if (!isRed(top) || !isBlue(left)) {
      const name = (c: Rgb) => (isRed(c) ? "red" : "blue");
      throw new Error(`issue-6470: want red on top and blue on the left, got ${name(top)} and ${name(left)}`);
    }
    console.log("issue-6470: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
