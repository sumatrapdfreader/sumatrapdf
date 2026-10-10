// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6266
//
// Some fixed-layout EPUBs wrap each page JPEG in an <svg width="100%"
// height="100%"> with no viewBox, and the <image> has no width or height.
// MuPDF skipped that image, so the page was a blank letter-size sheet.
//
// Two chapters, each a tall image: red on top, blue on the bottom. Fit-page
// must show both. The book is generated here; the reporter's manga is
// copyrighted and is not a fixture.
//
// Run: bun tests/issue-6266.ts [--no-build]

import { deflateSync } from "node:zlib";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, EXE, runStandalone, tmpPath, pngChunk, makeZip } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";

const IMG_W = 480;
const IMG_H = 960;

// Top half red, bottom half blue. A blank page has neither.
function makeSplitPng(): Buffer {
  const w = IMG_W;
  const h = IMG_H;
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  const stride = 1 + w * 3;
  const raw = Buffer.alloc(stride * h);
  for (let y = 0; y < h; y++) {
    const row = y * stride;
    raw[row] = 0;
    const red = y < h / 2;
    for (let x = 0; x < w; x++) {
      const i = row + 1 + x * 3;
      raw[i] = red ? 255 : 0;
      raw[i + 1] = 0;
      raw[i + 2] = red ? 0 : 255;
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

function chapterHtml(n: number): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>p${n}</title>` +
    `<style>html,body{margin:0;padding:0}div,svg{margin:0;padding:0}</style>` +
    `</head><body><div>` +
    `<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="100%" height="100%">` +
    `<image xlink:href="../images/split.png"/>` +
    `</svg></div></body></html>`
  );
}

function makeEpub(png: Buffer): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OPS/standard.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">urn:uuid:issue-6266</dc:identifier>` +
    `<dc:title>svg wrapped image</dc:title><dc:language>en</dc:language>` +
    `<meta property="rendition:layout">pre-paginated</meta>` +
    `</metadata><manifest>` +
    `<item id="c1" href="text/c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="c2" href="text/c2.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="img" href="images/split.png" media-type="image/png"/>` +
    `</manifest><spine><itemref idref="c1"/><itemref idref="c2"/></spine></package>`;
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OPS/standard.opf", data: enc.encode(opf) },
    { name: "OPS/text/c1.xhtml", data: enc.encode(chapterHtml(1)) },
    { name: "OPS/text/c2.xhtml", data: enc.encode(chapterHtml(2)) },
    { name: "OPS/images/split.png", data: png, store: true },
  ]);
}

function countBlue(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (b >= 180 && r <= 80 && g <= 80) {
      n++;
    }
  }
  return n;
}

function countRed(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r >= 180 && g <= 80 && b <= 80) {
      n++;
    }
  }
  return n;
}

async function renderPageColors(
  client: ControlClient,
  path: string,
  pageNo: number,
): Promise<{ red: number; blue: number; w: number; h: number }> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [path, pageNo]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6266: render failed: ${raw.trim()}`);
  }
  const size = /size=(\d+)x(\d+)/.exec(raw);
  return {
    red: Number(/red=(\d+)/.exec(raw)?.[1] ?? 0),
    blue: Number(/blue=(\d+)/.exec(raw)?.[1] ?? 0),
    w: Number(size?.[1] ?? 0),
    h: Number(size?.[2] ?? 0),
  };
}

async function showPage(
  client: ControlClient,
  frame: number,
  pageCmd: number | null,
): Promise<{ red: number; blue: number; w: number; h: number }> {
  if (pageCmd != null) {
    sendCommand(frame, pageCmd);
  }
  await client.waitForRenderIdle(30000);
  await client.setNotificationsEnabled(false);
  const canvas = findCanvas(frame);
  if (!canvas) {
    throw new Error("issue-6266: no canvas");
  }
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("issue-6266: capture failed");
  }
  return { red: countRed(cap.data), blue: countBlue(cap.data), w: cap.w, h: cap.h };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6266");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epubPath = join(dir, "issue-6266.epub");
  writeFileSync(epubPath, makeEpub(makeSplitPng()));

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-6266: no frame");
      }
      sendCommand(frame, cmdId("CmdZoomFitPageAndSinglePage"));
      // macOS has no window DC. A skipped SVG image leaves the engine page blank.
      if (IS_MAC) {
        for (const pageNo of [1, 2]) {
          const page = await renderPageColors(client, epubPath, pageNo);
          console.log(`issue-6266 page ${pageNo}: red=${page.red} blue=${page.blue} ${page.w}x${page.h}`);
          if (page.red < 200) {
            throw new Error(
              `issue-6266: page ${pageNo} did not paint the red half (red=${page.red} blue=${page.blue})`,
            );
          }
          if (page.blue < 200) {
            throw new Error(
              `issue-6266: page ${pageNo} did not paint the blue half (red=${page.red} blue=${page.blue})`,
            );
          }
        }
        return;
      }
      const page1 = await showPage(client, frame, null);
      console.log(`issue-6266 page 1: red=${page1.red} blue=${page1.blue} ${page1.w}x${page1.h}`);
      if (page1.red < 200) {
        throw new Error(`issue-6266: page 1 did not paint the red half (red=${page1.red} blue=${page1.blue})`);
      }
      if (page1.blue < 200) {
        throw new Error(`issue-6266: page 1 did not paint the blue half (red=${page1.red} blue=${page1.blue})`);
      }

      const page2 = await showPage(client, frame, cmdId("CmdGoToNextPage"));
      console.log(`issue-6266 page 2: red=${page2.red} blue=${page2.blue} ${page2.w}x${page2.h}`);
      if (page2.red < 200) {
        throw new Error(`issue-6266: page 2 did not paint the red half (red=${page2.red} blue=${page2.blue})`);
      }
      if (page2.blue < 200) {
        throw new Error(`issue-6266: page 2 did not paint the blue half (red=${page2.red} blue=${page2.blue})`);
      }
    },
    ["-view", "single page", "-zoom", "fit page", epubPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
