// Test for issue #5844: transparency in an image has to show the document
// background, not a backdrop baked into the page.
//
// EngineImages has three render paths and they used to disagree about what is
// behind a transparent pixel: the GDI+ scaler cleared to white and the
// nearest-neighbor fallback filled the pixmap white, but the mupdf fast path
// (unrotated, no tiling - the common case) handed the alpha channel back
// untouched, so the canvas drew the transparent parts black. Rotating the image
// switched paths and the very same file changed colour.
//
// All three now keep the alpha and the canvas composites the page over the
// document background, so transparency shows whatever is behind the page -
// black by default for images, the user's colour, or the checkered pattern.
//
// The test opens a PNG that is red on the left and fully transparent on the
// right and checks that the transparent half matches the canvas background -
// both as opened (mupdf path) and after rotating (fallback path).

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { cmdId, tmpPath } from "./util.ts";
import { findCanvas, launchControlled, killAndWait, sendCommandSync } from "./win-automation.ts";
import { captureWindowPixels } from "./winapi.ts";

function crc32(buf: Buffer): number {
  let c: number;
  let crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    c = (crc ^ buf[n]!) & 0xff;
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

// RGBA PNG: opaque red on the left half, alpha = 0 on the right half
function makeHalfTransparentPng(w: number, h: number): Buffer {
  const raw = Buffer.alloc((w * 4 + 1) * h);
  for (let y = 0; y < h; y++) {
    const row = y * (w * 4 + 1);
    raw[row] = 0; // filter: none
    for (let x = 0; x < w; x++) {
      const i = row + 1 + x * 4;
      const opaque = x < w / 2;
      raw[i] = opaque ? 220 : 0;
      raw[i + 1] = 0;
      raw[i + 2] = opaque ? 40 : 0;
      raw[i + 3] = opaque ? 255 : 0;
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 6; // color type: truecolor + alpha
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

type Color = { b: number; g: number; r: number };

// average color of a small block at (fx, fy) inside the page. The page is found
// as the bounding box of everything that isn't the (black) image-engine
// backdrop - but with the bug the transparent half is black too and melts into
// that backdrop, so only half of the page is inside the box. The image is
// square, so the longer side of the box is the page's side either way.
function pageColorAt(canvas: number, fx: number, fy: number): Color {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("issue-5844: could not capture the canvas");
  }
  const { w, h, data } = cap;
  const lit = (x: number, y: number) => {
    const i = (y * w + x) * 4;
    return data[i]! + data[i + 1]! + data[i + 2]! > 120;
  };
  let x0 = w;
  let y0 = h;
  let x1 = -1;
  let y1 = -1;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      if (!lit(x, y)) {
        continue;
      }
      x0 = Math.min(x0, x);
      y0 = Math.min(y0, y);
      x1 = Math.max(x1, x);
      y1 = Math.max(y1, y);
    }
  }
  if (x1 - x0 < 40 || y1 - y0 < 40) {
    throw new Error(`issue-5844: no page found on the canvas (bbox ${x0},${y0}-${x1},${y1})`);
  }
  const side = Math.max(x1 - x0, y1 - y0);
  const cx = Math.round(x0 + side * fx);
  const cy = Math.round(y0 + side * fy);
  let b = 0;
  let g = 0;
  let r = 0;
  let n = 0;
  for (let y = cy - 4; y <= cy + 4; y++) {
    for (let x = cx - 4; x <= cx + 4; x++) {
      const i = (y * w + x) * 4;
      b += data[i]!;
      g += data[i + 1]!;
      r += data[i + 2]!;
      n++;
    }
  }
  return { b: b / n, g: g / n, r: r / n };
}

function fmt(c: Color): string {
  return `rgb(${c.r.toFixed(0)},${c.g.toFixed(0)},${c.b.toFixed(0)})`;
}

// average colour of a block at a fraction of the whole canvas, for sampling the
// background outside the page
function canvasColorAt(canvas: number, fx: number, fy: number): Color {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("issue-5844: could not capture the canvas");
  }
  const { w, h, data } = cap;
  const cx = Math.round(w * fx);
  const cy = Math.round(h * fy);
  let b = 0;
  let g = 0;
  let r = 0;
  let n = 0;
  for (let y = cy - 3; y <= cy + 3; y++) {
    for (let x = cx - 3; x <= cx + 3; x++) {
      const i = (y * w + x) * 4;
      b += data[i]!;
      g += data[i + 1]!;
      r += data[i + 2]!;
      n++;
    }
  }
  return { b: b / n, g: g / n, r: r / n };
}

function sameColor(a: Color, b: Color): boolean {
  return Math.abs(a.r - b.r) < 24 && Math.abs(a.g - b.g) < 24 && Math.abs(a.b - b.b) < 24;
}

function isRed(c: Color): boolean {
  return c.r > 150 && c.g < 90 && c.b < 90;
}

export async function testit(): Promise<void> {
  const png = tmpPath("issue-5844.png");
  writeFileSync(png, makeHalfTransparentPng(400, 400));

  const { proc, client, frame } = await launchControlled([png]);
  try {
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-5844: could not find the canvas window");
    }

    // the canvas background, sampled well outside the page
    const bg = canvasColorAt(canvas, 0.02, 0.02);

    // as opened: mupdf fast path (no rotation, whole page in one bitmap)
    const opaque = pageColorAt(canvas, 0.25, 0.5);
    const transparent = pageColorAt(canvas, 0.75, 0.5);
    if (!isRed(opaque)) {
      throw new Error(`issue-5844: the opaque half of the png isn't red: ${fmt(opaque)}`);
    }
    if (!sameColor(transparent, bg)) {
      throw new Error(
        `issue-5844: the transparent half of the png rendered ${fmt(transparent)}, want the ` +
          `document background ${fmt(bg)}: the page was drawn with a backdrop baked in ` +
          `instead of being composited over the background`,
      );
    }

    // rotated: the nearest-neighbor fallback path. Rotating right puts the
    // transparent half at the bottom. The two paths have to agree.
    sendCommandSync(frame, cmdId("CmdRotateRight"));
    await client.waitForRenderIdle();
    const opaqueRot = pageColorAt(canvas, 0.5, 0.25);
    const transparentRot = pageColorAt(canvas, 0.5, 0.75);
    if (!isRed(opaqueRot)) {
      throw new Error(`issue-5844: after rotating, the opaque half isn't red: ${fmt(opaqueRot)}`);
    }
    if (!sameColor(transparentRot, bg)) {
      throw new Error(
        `issue-5844: after rotating, the transparent half rendered ${fmt(transparentRot)}, ` +
          `want the document background ${fmt(bg)}`,
      );
    }
    console.log(
      `  transparent png shows the background ${fmt(bg)} both ways ` +
        `(${fmt(transparent)} / ${fmt(transparentRot)}) ✓`,
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
