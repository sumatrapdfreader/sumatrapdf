// RTF documents open as reflowable documents: text in the document's and the
// fonts' code pages and in \u, headings as the table of contents, and RTF
// saved under another extension (Word does that with .doc).
//
// Run: bun tests/rtf.ts [--no-build] [-ng]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const kCenteredRtf = String.raw`{\rtf1\ansi\trowd\trqc\clbrdrl\brdrs\clbrdrr\brdrs\cellx1440\intbl x\cell\row}`;
const kMaxCenterOffPx = 3;

// a \trqc table is as far from the page's right edge as from its left
async function testCenteredTable(dir: string): Promise<void> {
  if (USE_NG) {
    return;
  }
  const rtf = join(dir, "centered.rtf");
  writeFileSync(rtf, kCenteredRtf, "latin1");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", rtf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const cap = captureWindowPixels(findCanvas(frame));
    if (!cap) {
      throw new Error("rtf: canvas capture failed");
    }
    const rgb = (x: number, y: number) => {
      const i = (y * cap.w + x) * 4;
      return [cap.data[i]!, cap.data[i + 1]!, cap.data[i + 2]!];
    };
    const isWhite = (x: number, y: number) => rgb(x, y).every((c) => c > 250);
    const isDark = (x: number, y: number) => rgb(x, y).every((c) => c < 130);

    // the page: the white span of the row where it's widest, in the empty lower half
    const midY = cap.h - Math.floor(cap.h / 4);
    let pageL = 0;
    while (pageL < cap.w && !isWhite(pageL, midY)) {
      pageL++;
    }
    let pageR = cap.w - 1;
    while (pageR > pageL && !isWhite(pageR, midY)) {
      pageR--;
    }
    let minX = cap.w;
    let maxX = -1;
    for (let y = 0; y < midY; y++) {
      for (let x = pageL; x <= pageR; x++) {
        if (isDark(x, y)) {
          minX = Math.min(minX, x);
          maxX = Math.max(maxX, x);
        }
      }
    }
    if (maxX < 0) {
      throw new Error("rtf: the centered table didn't draw");
    }
    const off = (minX + maxX) / 2 - (pageL + pageR) / 2;
    if (Math.abs(off) > kMaxCenterOffPx) {
      throw new Error(`rtf: table ${minX}..${maxX} is ${off}px off the center of page ${pageL}..${pageR}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// ascii-only RTF source: cp1251 bytes are written as \'hh
const kRtf = String.raw`{\rtf1\ansi\ansicpg1252\deff0
{\fonttbl{\f0\froman Times New Roman;}{\f1\fswiss\fcharset204 Arial;}}
{\info{\title RTF test}}
\pard\outlinelevel0\qc\b First chapter\b0\par
\pard Plain paragraph with {\i italic} text.\par
\pard {\f1 \'cf\'f0\'e8\'e2\'e5\'f2} and \uc1′0?⊐9? here.\par
\trowd\cellx2000\cellx4000\intbl cellone\cell celltwo\cell\row
\pard {\field{\*\fldinst HYPERLINK "https://www.sumatrapdfreader.org"}{\fldrslt linktext}}\par
\page
\pard\outlinelevel0 Second chapter\par
\pard Last words.\par
}`;

const kNeedles = ["italic", "Привет", "你好", "celltwo", "linktext", "Last words"];
const kToc = ["First chapter|page=1", "Second chapter|page=2"];

export async function testit(): Promise<void> {
  const dir = tmpPath("rtf");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const rtf = join(dir, "test.rtf");
  writeFileSync(rtf, kRtf, "latin1");
  const doc = join(dir, "saved-by-word.doc");
  copyFileSync(rtf, doc);

  await withControlledSumatra(EXE, async (client) => {
    for (const needle of kNeedles) {
      const res = String((await client.request(ControlCommand.TestSearch, [rtf, needle]))[1] ?? "");
      if (!res.startsWith("FOUND")) {
        throw new Error(`rtf: '${needle}' not found: ${res.trim()}`);
      }
    }

    const [exitCode, raw] = await client.request(ControlCommand.TestGetToc, [rtf]);
    const toc = String(raw ?? "")
      .trim()
      .split("\n");
    if (exitCode !== 0 || toc.join(",") !== kToc.join(",")) {
      throw new Error(`rtf: TOC is '${toc.join(",")}', want '${kToc.join(",")}'`);
    }

    const res = String((await client.request(ControlCommand.TestSearch, [doc, "celltwo"]))[1] ?? "");
    if (!res.startsWith("FOUND")) {
      throw new Error(`rtf: RTF named .doc didn't open: ${res.trim()}`);
    }
  });
  await testCenteredTable(dir);
  console.log("rtf: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
