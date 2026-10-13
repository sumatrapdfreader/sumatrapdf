// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6330
//
// A fixed-layout EPUB wraps each page JPEG in an <svg>. MuPDF turns that svg
// into a scalable image but rasterized it at a fixed 96 dpi of its layout
// size, so zooming in scaled up a small bitmap and the page was blurry.
//
// The page is a 300x400 pt svg holding a 600x800 px image of 1 px black and
// white stripes under a solid black top band. At 200% a sharp render is 1:1
// black and white, while the old 400x533 raster scaled up is gray.
//
// Save Image / copy also look through the svg: the bitmap is the raster at
// its own 600x800, upright, and the original PNG bytes are available.
//
// Run: bun tests/issue-6330.ts [--no-build]

import { deflateSync } from "node:zlib";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, pngChunk, makeZip } from "./util.ts";

const ZOOM_PERCENT = 200;
const SVG_W = 300;
const SVG_H = 400;
const IMG_W = (SVG_W * ZOOM_PERCENT) / 100;
const IMG_H = (SVG_H * ZOOM_PERCENT) / 100;
const BAND_H = 100;

// 8-bit gray PNG: a black band on top, below it columns alternate black and
// white, so the corners tell a flipped bitmap from an upright one
function makeStripesPng(w: number, h: number): Buffer {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 0;
  const stride = 1 + w;
  const raw = Buffer.alloc(stride * h);
  for (let y = 0; y < h; y++) {
    const row = y * stride;
    raw[row] = 0;
    for (let x = 0; x < w; x++) {
      raw[row + 1 + x] = y >= BAND_H && x & 1 ? 255 : 0;
    }
  }
  const sig = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
  return Buffer.concat([
    sig,
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function pageHtml(w: number, h: number): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>p1</title>` +
    `<style>html,body{margin:0;padding:0;font-size:0}svg{margin:0;padding:0}</style>` +
    `</head><body><div>` +
    `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink"` +
    ` width="${SVG_W}" height="${SVG_H}" viewBox="0 0 ${w} ${h}">` +
    `<image width="${w}" height="${h}" xlink:href="../images/stripes.png"/>` +
    `</svg></div></body></html>`
  );
}

function makeEpub(w: number, h: number): Buffer {
  const container =
    `<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">` +
    `<rootfiles><rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>` +
    `</container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="uid">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="uid">issue-6330</dc:identifier>` +
    `<dc:title>issue 6330</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest><item id="p1" href="text/p1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="img" href="images/stripes.png" media-type="image/png"/></manifest>` +
    `<spine><itemref idref="p1"/></spine></package>`;
  const enc = new TextEncoder();
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OPS/content.opf", data: enc.encode(opf) },
    { name: "OPS/text/p1.xhtml", data: enc.encode(pageHtml(w, h)) },
    { name: "OPS/images/stripes.png", data: makeStripesPng(w, h), store: true },
  ]);
}

type Render = { w: number; h: number; nonWhite: number; gray: number; box: number[]; raw: string };

async function render(client: ControlClient, epubPath: string): Promise<Render> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [epubPath, 1, ZOOM_PERCENT]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6330: render failed: ${raw}`);
  }
  const size = /size=(\d+)x(\d+)/.exec(raw);
  const box = (/box=([\d,]+)/.exec(raw)?.[1] ?? "0,0,0,0").split(",").map(Number);
  return {
    w: Number(size?.[1] ?? 0),
    h: Number(size?.[2] ?? 0),
    nonWhite: Number(/nonwhite=(\d+)/.exec(raw)?.[1] ?? 0),
    gray: Number(/gray=(\d+)/.exec(raw)?.[1] ?? 0),
    box,
    raw,
  };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6330");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epubPath = join(dir, "issue-6330.epub");

  writeFileSync(epubPath, makeEpub(IMG_W, IMG_H));

  await withControlledSumatra(EXE, async (client) => {
    const page = await render(client, epubPath);
    const [x0, y0, x1, y1] = page.box as [number, number, number, number];
    const boxW = x1 - x0;
    const boxH = y1 - y0;
    console.log(`issue-6330: page ${page.w}x${page.h} box ${boxW}x${boxH} nonwhite=${page.nonWhite} gray=${page.gray}`);
    if (Math.abs(boxW - IMG_W) > 2 || Math.abs(boxH - IMG_H) > 2) {
      throw new Error(`issue-6330: want a ${IMG_W}x${IMG_H} image box: ${page.raw}`);
    }
    // a blurred render is mid-gray nearly everywhere
    if (page.gray > (boxW * boxH) / 10) {
      throw new Error(`issue-6330: the page is blurry: ${page.raw}`);
    }

    const res = await client.request(ControlCommand.TestImageOrientation, [epubPath, 1]);
    const raw = String(res[1] ?? "").trim();
    if (res[0] !== 0) {
      throw new Error(`issue-6330: image element failed: ${raw}`);
    }
    const m = /w=(\d+) h=(\d+) data=(\d+) tl=(\w+) tr=(\w+) bl=(\w+) br=(\w+)/.exec(raw);
    if (!m) {
      throw new Error(`issue-6330: unexpected image result: ${raw}`);
    }
    console.log(`issue-6330: image ${raw}`);
    const [, w, h, data, tl, tr, bl, br] = m;
    if (Number(w) !== IMG_W || Number(h) !== IMG_H) {
      throw new Error(`issue-6330: want the ${IMG_W}x${IMG_H} raster, not the svg render: ${raw}`);
    }
    if (Number(data) === 0) {
      throw new Error(`issue-6330: the original image bytes are not available: ${raw}`);
    }
    // black band on top, bottom-right is the last (white) stripe column
    if (tl !== "000000" || tr !== "000000" || bl !== "000000" || br !== "ffffff") {
      throw new Error(`issue-6330: the raster is not upright: ${raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
