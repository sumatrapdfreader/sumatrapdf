// Copying a selection joins the wrapped lines of a paragraph with a space and
// keeps a line break where a paragraph or a heading ends (#5793; UserJot
// "Selecting multiline text without carriage returns"). A short line in
// ragged-right text is a wrap, not a paragraph end: the next line's first
// word didn't fit on it.
//
// Run: bun tests/copy-join-lines.ts [--no-build]

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { getClipboardText } from "./winapi.ts";

const kLeading = 12;

// lines of 10pt Helvetica at 72 pt from the left; "" adds a paragraph gap
function makePdf(lines: string[]): string {
  let y = 700;
  let ops = "BT /F1 10 Tf\n";
  for (const l of lines) {
    if (l === "") {
      y -= 14;
      continue;
    }
    ops += `1 0 0 1 72 ${y} Tm (${l}) Tj\n`;
    y -= kLeading;
  }
  ops += "ET\n";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${ops.length} >>\nstream\n${ops}endstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ]);
}

type Case = { name: string; lines: string[]; want: string[] };

const cases: Case[] = [
  {
    name: "justified paragraphs",
    lines: [
      "The quick brown fox jumps over the lazy dog near the riverbank today",
      "while the farmer watches from the old wooden fence beside the barn",
      "and the sun sets.",
      "",
      "A second paragraph starts here after a clear gap between the blocks",
      "and continues on this line with more words to fill the full width up",
      "to its end.",
    ],
    want: [
      "The quick brown fox jumps over the lazy dog near the riverbank today while the farmer watches from the old wooden fence beside the barn and the sun sets.",
      "A second paragraph starts here after a clear gap between the blocks and continues on this line with more words to fill the full width up to its end.",
    ],
  },
  {
    // the second line is short because "internationalization" didn't fit after it
    name: "ragged right",
    lines: [
      "The quick brown fox jumps over the lazy dog near the riverbank today",
      "while the farmer watches from the old fence by the barn",
      "internationalization of farming is the topic for the long afternoon",
      "and the sun sets.",
    ],
    want: [
      "The quick brown fox jumps over the lazy dog near the riverbank today while the farmer watches from the old fence by the barn internationalization of farming is the topic for the long afternoon and the sun sets.",
    ],
  },
  {
    name: "heading",
    lines: [
      "Chapter One",
      "The quick brown fox jumps over the lazy dog near the riverbank today",
      "while the farmer watches from the old wooden fence beside the barn.",
    ],
    want: [
      "Chapter One",
      "The quick brown fox jumps over the lazy dog near the riverbank today while the farmer watches from the old wooden fence beside the barn.",
    ],
  },
  {
    // no gap between the paragraphs: the short last line ends the first one
    name: "paragraphs without a gap",
    lines: [
      "The quick brown fox jumps over the lazy dog near the riverbank today",
      "and the sun sets.",
      "A second paragraph follows at once on the next line with no spacing",
      "and ends here.",
    ],
    want: [
      "The quick brown fox jumps over the lazy dog near the riverbank today and the sun sets.",
      "A second paragraph follows at once on the next line with no spacing and ends here.",
    ],
  },
];

async function copyPage(file: string): Promise<string> {
  const { proc, client, frame } = await launchControlled([file]);
  try {
    await client.waitForRenderIdle();
    await client.seedTextSelection(1);
    sendCommandSync(frame, cmdId("CmdCopySelection"));
    return (await getClipboardText(2000)).replace(/[\r\n]+$/, "");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("copy-join-lines");
  mkdirSync(dir, { recursive: true });
  const failed: string[] = [];
  for (const c of cases) {
    const file = join(dir, `${c.name.replaceAll(" ", "-")}.pdf`);
    writeFileSync(file, makePdf(c.lines), "latin1");
    const got = await copyPage(file);
    const want = c.want.join("\r\n");
    if (got !== want) {
      failed.push(`${c.name}:\n  got  ${JSON.stringify(got)}\n  want ${JSON.stringify(want)}`);
    }
  }
  if (failed.length > 0) {
    throw new Error(`copy-join-lines:\n${failed.join("\n")}`);
  }
  console.log("copy-join-lines: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
