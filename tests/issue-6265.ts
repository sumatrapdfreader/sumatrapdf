// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6265
//
// A fixed-layout EPUB sets a viewport larger than the A5 reflow page. Each
// spine item is its own chapter. The page MuPDF lays out is the viewport, but
// the engine used to keep the A5 size it asked for and clip the paint to that,
// so a full-bleed image showed only its top.
//
// Two chapters, each a tall image: red on top, blue on the bottom. Fit-page
// must show the blue. The book is generated here; the reporter's photo EPUB
// is copyrighted and is not a fixture.
//
// Run: bun tests/issue-6265.ts [--no-build]

import { deflateSync } from "node:zlib";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, EXE, runStandalone, tmpPath, pngChunk, makeZip } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";

const VIEW_W = 900;
const VIEW_H = 1600;

// 4x8, top half red, bottom half blue. Displayed at the viewport size, so a
// clip to the A5 page keeps only the red.
function makeSplitPng(): Buffer {
  const w = 4;
  const h = 8;
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
    `<meta name="viewport" content="width=${VIEW_W}, height=${VIEW_H}"/>` +
    `</head><body style="margin:0">` +
    `<img src="split.png" alt="" style="position:absolute;left:0;top:0;width:${VIEW_W}px;height:${VIEW_H}px"/>` +
    `</body></html>`
  );
}

function makeEpub(png: Buffer): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">urn:uuid:issue-6265</dc:identifier>` +
    `<dc:title>fixed layout image</dc:title><dc:language>en</dc:language>` +
    `<meta property="rendition:layout">pre-paginated</meta>` +
    `</metadata><manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="c2" href="c2.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="img" href="split.png" media-type="image/png"/>` +
    `</manifest><spine><itemref idref="c1"/><itemref idref="c2"/></spine></package>`;
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(chapterHtml(1)) },
    { name: "OEBPS/c2.xhtml", data: enc.encode(chapterHtml(2)) },
    { name: "OEBPS/split.png", data: png, store: true },
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
    throw new Error(`issue-6265: render failed: ${raw.trim()}`);
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
    throw new Error("issue-6265: no canvas");
  }
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("issue-6265: capture failed");
  }
  return { red: countRed(cap.data), blue: countBlue(cap.data), w: cap.w, h: cap.h };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6265");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epubPath = join(dir, "issue-6265.epub");
  writeFileSync(epubPath, makeEpub(makeSplitPng()));

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-6265: no frame");
      }
      sendCommand(frame, cmdId("CmdZoomFitPageAndSinglePage"));
      // macOS has no window DC. The engine page bitmap is what was clipped.
      if (IS_MAC) {
        for (const pageNo of [1, 2]) {
          const page = await renderPageColors(client, epubPath, pageNo);
          console.log(`issue-6265 page ${pageNo}: red=${page.red} blue=${page.blue} ${page.w}x${page.h}`);
          if (page.red < 200) {
            throw new Error(
              `issue-6265: page ${pageNo} did not paint the red half (red=${page.red} blue=${page.blue})`,
            );
          }
          if (page.blue < 200) {
            throw new Error(`issue-6265: page ${pageNo} clipped the image (red=${page.red} blue=${page.blue})`);
          }
        }
        return;
      }
      const page1 = await showPage(client, frame, null);
      console.log(`issue-6265 page 1: red=${page1.red} blue=${page1.blue} ${page1.w}x${page1.h}`);
      if (page1.red < 200) {
        throw new Error(`issue-6265: page 1 did not paint the red half (red=${page1.red} blue=${page1.blue})`);
      }
      if (page1.blue < 200) {
        throw new Error(`issue-6265: page 1 clipped the image (red=${page1.red} blue=${page1.blue})`);
      }

      const page2 = await showPage(client, frame, cmdId("CmdGoToNextPage"));
      console.log(`issue-6265 page 2: red=${page2.red} blue=${page2.blue} ${page2.w}x${page2.h}`);
      if (page2.red < 200) {
        throw new Error(`issue-6265: page 2 did not paint the red half (red=${page2.red} blue=${page2.blue})`);
      }
      if (page2.blue < 200) {
        throw new Error(`issue-6265: page 2 clipped the image (red=${page2.red} blue=${page2.blue})`);
      }
    },
    ["-view", "single page", "-zoom", "fit page", epubPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
