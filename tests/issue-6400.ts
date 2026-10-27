// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6400
//
// A fixed-layout AZW3 whose kindle:embed pages include the EXTH cover image
// must show the cover once, not as an extra page before the image pages.
//
// Fixture: a fixed-layout AZW3 with pages red (the cover), blue, red, written
// to tests/tmp using the JPEGs of tests/issue-4315.azw3.
//
// Run: bun tests/issue-6400.ts [--no-build]

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SRC_AZW3 = join(import.meta.dir, "issue-4315.azw3");
const kExthCover = 201;

function pdbRecords(d: Buffer): Buffer[] {
  const n = d.readUInt16BE(76);
  const offs: number[] = [];
  for (let i = 0; i < n; i++) {
    offs.push(d.readUInt32BE(78 + i * 8));
  }
  offs.push(d.length);
  return offs.slice(0, n).map((off, i) => d.subarray(off, offs[i + 1]));
}

// KF8 fixed-layout book: each page is a kindle:embed image, EXTH 201 = image 0
function makeAzw3(red: Buffer, blue: Buffer): Buffer {
  const html = Buffer.from(
    '<html><head><meta name="viewport" content="width=400, height=600"/></head><body>' +
      '<div><img src="kindle:embed:0001?mime=image/jpeg"/></div>' +
      '<div><img src="kindle:embed:0002?mime=image/jpeg"/></div>' +
      '<div><img src="kindle:embed:0003?mime=image/jpeg"/></div></body></html>',
    "ascii",
  );

  const exth = Buffer.alloc(12 + 12);
  Buffer.from("EXTH").copy(exth, 0);
  exth.writeUInt32BE(exth.length, 4);
  exth.writeUInt32BE(1, 8);
  exth.writeUInt32BE(kExthCover, 12);
  exth.writeUInt32BE(12, 16);
  exth.writeUInt32BE(0, 20);

  const mobi = Buffer.alloc(116);
  Buffer.from("MOBI").copy(mobi, 0);
  mobi.writeUInt32BE(116, 4);
  mobi.writeUInt32BE(2, 8); // book
  mobi.writeUInt32BE(65001, 12); // utf-8
  mobi.writeUInt32BE(1, 16);
  mobi.writeUInt32BE(8, 20); // KF8
  mobi.writeUInt32BE(2, 64); // firstNonBookRec
  mobi.writeUInt32BE(16 + 116 + exth.length, 68); // fullNameOffset in rec0
  mobi.writeUInt32BE(4, 72);
  mobi.writeUInt32BE(2, 92); // imageFirstRec
  mobi.writeUInt32BE(0x40, 112); // has EXTH

  const palm = Buffer.alloc(16);
  palm.writeUInt16BE(1, 0); // no compression
  palm.writeUInt32BE(html.length, 4);
  palm.writeUInt16BE(1, 8); // text record count
  palm.writeUInt16BE(4096, 10);

  const recs = [Buffer.concat([palm, mobi, exth, Buffer.from("TEST")]), html, red, blue, red];
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

function parseColors(raw: string): { red: number; blue: number } {
  const red = /red=(\d+)/.exec(raw);
  const blue = /blue=(\d+)/.exec(raw);
  return { red: red ? +red[1]! : 0, blue: blue ? +blue[1]! : 0 };
}

export async function testit(): Promise<void> {
  if (!existsSync(SRC_AZW3)) {
    throw new Error(`issue-6400: fixture not found: ${SRC_AZW3}`);
  }
  const src = pdbRecords(readFileSync(SRC_AZW3));
  const azw3 = tmpPath("issue-6400.azw3");
  writeFileSync(azw3, makeAzw3(src[2]!, src[3]!));

  await withControlledSumatra(
    EXE,
    async (client) => {
      // page numbers are only final once every chapter is laid out
      const deadline = Date.now() + 10_000;
      for (;;) {
        try {
          const info = await client.chapterInfo();
          if (info.laidOut >= info.chapterCount) {
            if (info.pageCount !== 3) {
              throw new Error(`issue-6400: book should have 3 pages, got ${info.pageCount}`);
            }
            break;
          }
        } catch (e) {
          if (String(e).includes("issue-6400") || Date.now() > deadline) {
            throw e;
          }
        }
        if (Date.now() > deadline) {
          throw new Error("issue-6400: layout did not finish");
        }
        await new Promise((r) => setTimeout(r, 50));
      }

      const want = ["red", "blue", "red"];
      for (let pageNo = 1; pageNo <= want.length; pageNo++) {
        const res = await client.request(ControlCommand.TestRenderPageColors, [azw3, pageNo, 10]);
        const raw = String(res[1] ?? "");
        if (res[0] !== 0) {
          throw new Error(`issue-6400 page ${pageNo}: ${raw.trim()}`);
        }
        const c = parseColors(raw);
        const got = c.red > c.blue ? "red" : c.blue > c.red ? "blue" : "none";
        if (got !== want[pageNo - 1]) {
          throw new Error(`issue-6400: page ${pageNo} should be ${want[pageNo - 1]}, got ${got} (${raw.trim()})`);
        }
      }
    },
    [azw3],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
