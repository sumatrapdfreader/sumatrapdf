// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6420
//
// Han text with no font of its own uses a system fallback font. A bold heading
// that asked for it first must not make every later regular paragraph bold.
// Two EPUBs hold the same bold heading and regular paragraphs, in opposite
// order, so both pages must carry about the same amount of ink.

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { CONTAINER, OPF, zip } from "./ad-hoc-issue-4600.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// the Windows Han fallback, regular and bold
const YAHEI = "C:\\Windows\\Fonts\\msyh.ttc";

const CN = "中文排版测试默认字体".repeat(8);
const HEADING = `<h1 style="font-weight: bold">中文标题</h1>`;
const PARAS = `<p>${CN}</p>`.repeat(3);

// BITMAPFILEHEADER.bfOffBits, then BITMAPINFOHEADER.biWidth / biHeight / biBitCount
const BMP_PIXELS_OFF = 10;
const BMP_WIDTH_OFF = 18;
const BMP_HEIGHT_OFF = 22;
const BMP_BPP_OFF = 28;
const DARK = 128 * 3;

// bold paragraphs carry ~40% more ink; a correct render differs by a few %
const MAX_INK_RATIO = 1.15;

function makeEpub(name: string, body: string): string {
  const enc = new TextEncoder();
  const html =
    `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title>` +
    `</head><body>${body}</body></html>`;
  const data = zip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(CONTAINER), store: false },
    { name: "OEBPS/content.opf", data: enc.encode(OPF), store: false },
    { name: "OEBPS/s.css", data: enc.encode(""), store: false },
    { name: "OEBPS/c1.xhtml", data: enc.encode(html), store: false },
  ]);
  const path = join(tmpPath("issue-6420"), `${name}.epub`);
  writeFileSync(path, data);
  return path;
}

// dark pixels on page 1
async function inkOf(epub: string): Promise<number> {
  const appData = tmpPath("issue-6420-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestConvertToImages, [join(appData, "page-<N>.bmp"), "1"]);
      if (res[0] !== 0 || !String(res[1]).startsWith("OK")) {
        throw new Error(`issue-6420: no render for ${epub}: ${res}`);
      }
    },
    ["-appdata", appData, epub],
  );

  const bmp = readFileSync(join(appData, "page-1.bmp"));
  const bytesPerPx = bmp.readUInt16LE(BMP_BPP_OFF) / 8;
  const w = bmp.readInt32LE(BMP_WIDTH_OFF);
  const h = Math.abs(bmp.readInt32LE(BMP_HEIGHT_OFF));
  const stride = (w * bytesPerPx + 3) & ~3;
  const start = bmp.readUInt32LE(BMP_PIXELS_OFF);
  let ink = 0;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const p = start + y * stride + x * bytesPerPx;
      if (bmp[p]! + bmp[p + 1]! + bmp[p + 2]! < DARK) {
        ink++;
      }
    }
  }
  return ink;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || !existsSync(YAHEI)) {
    console.log("issue-6420: skipped, needs Windows with Microsoft YaHei");
    return;
  }
  mkdirSync(tmpPath("issue-6420"), { recursive: true });
  const headingFirst = await inkOf(makeEpub("heading-first", HEADING + PARAS));
  const headingLast = await inkOf(makeEpub("heading-last", PARAS + HEADING));
  const ratio = headingFirst / headingLast;
  console.log(`issue-6420: ink heading first ${headingFirst}, last ${headingLast}, ratio ${ratio.toFixed(3)}`);
  if (ratio > MAX_INK_RATIO || ratio < 1 / MAX_INK_RATIO) {
    throw new Error(`issue-6420: regular text weight depends on what came first (ratio ${ratio.toFixed(3)})`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
