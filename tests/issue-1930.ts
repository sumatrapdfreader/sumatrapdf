// #1930: converting a multi-frame GIF to PDF used to embed the whole GIF on
// every page. MuPDF composites that to the last frame, so PDF page 1 was a
// copy of the last GIF frame.
//
// Run: bun tests/issue-1930.ts [--no-build]

import { rmSync, writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const W = 16;
const H = 16;

// Uncompressed GIF LZW (9-bit codes, clear every 126 pixels) so a solid
// frame is just its palette index.
function uncompressedLzw(indexes: number[]): number[] {
  const clear = 256;
  const eoi = 257;
  const codeSize = 9;
  let acc = 0;
  let nacc = 0;
  const out: number[] = [];
  const put = (code: number) => {
    acc |= code << nacc;
    nacc += codeSize;
    while (nacc >= 8) {
      out.push(acc & 255);
      acc >>>= 8;
      nacc -= 8;
    }
  };
  put(clear);
  let sinceClear = 0;
  for (const idx of indexes) {
    put(idx);
    sinceClear++;
    if (sinceClear >= 126) {
      put(clear);
      sinceClear = 0;
    }
  }
  put(eoi);
  if (nacc > 0) {
    out.push(acc & 255);
  }
  return out;
}

function subBlocks(data: number[]): number[] {
  const out: number[] = [];
  for (let i = 0; i < data.length;) {
    const n = Math.min(255, data.length - i);
    out.push(n);
    for (let j = 0; j < n; j++) {
      out.push(data[i + j]!);
    }
    i += n;
  }
  out.push(0);
  return out;
}

// 16x16 GIF89a: frame 1 solid red, frame 2 solid blue.
function makeTwoFrameGif(): Buffer {
  const bytes: number[] = [];
  const push = (...b: number[]) => bytes.push(...b);
  push(0x47, 0x49, 0x46, 0x38, 0x39, 0x61);
  push(W & 255, W >> 8, H & 255, H >> 8);
  push(0xf7, 0, 0);
  push(255, 0, 0, 0, 0, 255);
  for (let i = 2; i < 256; i++) {
    push(0, 0, 0);
  }

  const frame = (index: number) => {
    push(0x21, 0xf9, 0x04, 0x08, 0x0a, 0x00, 0x00, 0x00);
    push(0x2c, 0, 0, 0, 0, W & 255, W >> 8, H & 255, H >> 8, 0);
    const pixels = new Array<number>(W * H).fill(index);
    push(8, ...subBlocks(uncompressedLzw(pixels)));
  };
  frame(0);
  frame(1);
  push(0x3b);
  return Buffer.from(bytes);
}

function parseColors(raw: string): { red: number; nonWhite: number; w: number; h: number; pages: number } {
  const m = /red=(\d+) nonwhite=(\d+) size=(\d+)x(\d+) pages=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-1930: could not parse: ${raw}`);
  }
  return { red: +m[1]!, nonWhite: +m[2]!, w: +m[3]!, h: +m[4]!, pages: +m[5]! };
}

export async function testit(): Promise<void> {
  const gif = tmpPath("issue-1930.gif");
  const pdf = tmpPath("issue-1930.pdf");
  writeFileSync(gif, makeTwoFrameGif());
  rmSync(pdf, { force: true });

  await withControlledSumatra(EXE, async (client) => {
    const gifRes = await client.request(ControlCommand.TestRenderPageColors, [gif]);
    const gifRaw = String(gifRes[1] ?? "");
    if (gifRes[0] !== 0) {
      throw new Error(`issue-1930 gif: ${gifRaw.trim()}`);
    }
    const gifColors = parseColors(gifRaw);
    if (gifColors.pages !== 2) {
      throw new Error(`issue-1930: gif pages=${gifColors.pages}, want 2:\n${gifRaw}`);
    }
    if (gifColors.red < 100) {
      throw new Error(`issue-1930: gif page 1 is not red (red=${gifColors.red}):\n${gifRaw}`);
    }

    const conv = await client.request(ControlCommand.TestConvertToPdf, [gif, pdf]);
    const convRaw = String(conv[1] ?? "");
    if (conv[0] !== 0) {
      throw new Error(`issue-1930 convert: ${convRaw.trim()}`);
    }

    const pdfRes = await client.request(ControlCommand.TestRenderPageColors, [pdf]);
    const pdfRaw = String(pdfRes[1] ?? "");
    if (pdfRes[0] !== 0) {
      throw new Error(`issue-1930 pdf: ${pdfRaw.trim()}`);
    }
    const pdfColors = parseColors(pdfRaw);
    if (pdfColors.pages !== 2) {
      throw new Error(`issue-1930: pdf pages=${pdfColors.pages}, want 2:\n${pdfRaw}`);
    }
    if (pdfColors.red < 100) {
      throw new Error(`issue-1930: pdf page 1 is not the first GIF frame (red=${pdfColors.red}):\n${pdfRaw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
