// RTF documents open as reflowable documents: text in the document's and the
// fonts' code pages and in \u, headings as the table of contents, and RTF
// saved under another extension (Word does that with .doc).
//
// Run: bun tests/rtf.ts [--no-build] [-ng]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

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
  console.log("rtf: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
