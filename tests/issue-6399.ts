// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6399
//
// AZW3 fixed-layout books are shown as one image per page. The page must have
// the image's size and no border, or facing pages show a white gap.
// Reflowed ebook pages report what they draw as the content box, so Trim
// Empty Margins crops to it.
//
// Fixtures: tests/issue-4315.azw3 (two 400x600 JPEG pages), a one-line
// MOBI written to tests/tmp.
//
// Run: bun tests/issue-6399.ts [--no-build]

import { existsSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const AZW3 = join(import.meta.dir, "issue-4315.azw3");
const kNotReady = 2;

// uncompressed text-only MOBI (EngineMobi; mupdf would take a PalmDoc)
function makeMobi(html: string): Buffer {
  const body = Buffer.from(html, "ascii");
  const mobi = Buffer.alloc(116);
  Buffer.from("MOBI").copy(mobi, 0);
  mobi.writeUInt32BE(116, 4);
  mobi.writeUInt32BE(2, 8); // book
  mobi.writeUInt32BE(65001, 12); // utf-8
  mobi.writeUInt32BE(1, 16);
  mobi.writeUInt32BE(6, 20);
  mobi.writeUInt32BE(2, 64); // firstNonBookRec
  mobi.writeUInt32BE(16 + 116, 68); // fullNameOffset in rec0
  mobi.writeUInt32BE(4, 72);
  const palm = Buffer.alloc(16);
  palm.writeUInt16BE(1, 0); // no compression
  palm.writeUInt32BE(body.length, 4);
  palm.writeUInt16BE(1, 8); // text record count
  palm.writeUInt16BE(4096, 10);
  const recs = [Buffer.concat([palm, mobi, Buffer.from("TEST")]), body];
  const hdr = Buffer.alloc(78);
  Buffer.from("Test").copy(hdr, 0);
  Buffer.from("BOOKMOBI").copy(hdr, 60);
  hdr.writeUInt16BE(recs.length, 76);
  let off = 78 + recs.length * 8 + 2;
  const table: Buffer[] = [];
  for (let i = 0; i < recs.length; i++) {
    const row = Buffer.alloc(8);
    row.writeUInt32BE(off, 0);
    row[7] = i + 1;
    table.push(row);
    off += recs[i]!.length;
  }
  return Buffer.concat([hdr, ...table, Buffer.from([0, 0]), ...recs]);
}

function parseBox(raw: string, name: string): number[] {
  const m = new RegExp(`${name}=([-\\d.]+),([-\\d.]+),([-\\d.]+),([-\\d.]+)`).exec(raw);
  if (!m) {
    throw new Error(`issue-6399: no ${name} in: ${raw.trim()}`);
  }
  return [+m[1]!, +m[2]!, +m[3]!, +m[4]!];
}

// returns [media, content] boxes of page 1 as x,y,dx,dy
async function pageBoxes(path: string): Promise<number[][]> {
  return await withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 10_000;
      for (;;) {
        const res = await client.request(ControlCommand.TestPageBoxes, [1]);
        const raw = String(res[1] ?? "");
        if (res[0] === 0) {
          return [parseBox(raw, "engineMedia"), parseBox(raw, "engineContent")];
        }
        if (res[0] !== kNotReady || Date.now() > deadline) {
          throw new Error(`issue-6399: TestPageBoxes failed: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 50));
      }
    },
    [path],
  );
}

async function testImagePages(): Promise<void> {
  if (!existsSync(AZW3)) {
    throw new Error(`issue-6399: fixture not found: ${AZW3}`);
  }
  const [media, content] = await pageBoxes(AZW3);
  const ratio = media![2]! / media![3]!;
  if (Math.abs(ratio - 400 / 600) > 0.01) {
    throw new Error(`issue-6399: page should have the image's 2:3 shape, got ${media!.join(",")}`);
  }
  if (content!.join(",") !== media!.join(",")) {
    throw new Error(`issue-6399: page should have no border, media=${media!.join(",")} content=${content!.join(",")}`);
  }
}

async function testTextContentBox(): Promise<void> {
  const mobi = tmpPath("issue-6399.mobi");
  writeFileSync(mobi, makeMobi("<html><body><p>Short.</p></body></html>"));
  const [media, content] = await pageBoxes(mobi);
  const [mx, my, mdx, mdy] = media!;
  const [cx, cy, cdx, cdy] = content!;
  const inside = cx! >= mx! && cy! >= my! && cx! + cdx! <= mx! + mdx! && cy! + cdy! <= my! + mdy!;
  if (!inside || cdx! <= 0 || cdy! <= 0) {
    throw new Error(`issue-6399: bad content box ${content!.join(",")} for media ${media!.join(",")}`);
  }
  // one line of text: far shorter and narrower than the page
  if (cdy! > mdy! / 4 || cdx! > mdx! / 2) {
    throw new Error(`issue-6399: content box should hug the text, got ${content!.join(",")}`);
  }
}

export async function testit(): Promise<void> {
  await testImagePages();
  await testTextContentBox();
}

if (import.meta.main) {
  await runStandalone(testit);
}
