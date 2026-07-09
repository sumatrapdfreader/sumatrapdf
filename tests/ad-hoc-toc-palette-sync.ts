// Ad-hoc regression test for issue #5716: navigating to a bookmark via the
// Command Palette (the "*" / TOC mode) must move the selection in the Bookmarks
// (table-of-contents) panel too. Before the fix the page navigated but the tree
// selection stayed on the previous item.
//
// This is GUI automation (there's no -dbg-control hook for palette TOC
// navigation), so it lives as an ad-hoc test, not in tests/all.ts. Run directly:
//   bun tests/ad-hoc-toc-palette-sync.ts [--no-build]
//
// Reverting the fix (the selectInTree path in TableOfContents.cpp's GoToTocLink)
// makes the tree selection stay put, so this throws.

import { writeFileSync } from "node:fs";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { launchSumatra, waitForFrame, findChildByClass, sendCommand, pressEnter } from "./win-automation.ts";
import {
  sleep,
  moveWindow,
  treeGetSelection,
  enumWindows,
  getWindowPid,
  findChildWindow,
  sendText,
} from "./winapi.ts";

const CmdCommandPaletteTOC = cmdId("CmdCommandPaletteTOC");

// a 4-page PDF with 4 top-level bookmarks (one per page), /UseOutlines so the
// bookmarks panel opens automatically
function makeBookmarkedPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const titles = ["Chapter One", "Chapter Two", "Chapter Three", "Chapter Four"];
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R /Outlines 8 0 R /PageMode /UseOutlines >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R 6 0 R] /Count 4 >>");
  for (let i = 0; i < 4; i++) {
    const pageObj = 3 + i;
    const contentObj = 13 + i;
    const stream = `BT /F1 28 Tf 72 700 Td (Page ${i + 1} - ${titles[i]}) Tj ET`;
    body[contentObj] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
    body[pageObj] = enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
        `/Resources << /Font << /F1 7 0 R >> >> /Contents ${contentObj} 0 R >>`,
    );
  }
  body[7] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  body[8] = enc("<< /Type /Outlines /First 9 0 R /Last 12 0 R /Count 4 >>");
  for (let i = 0; i < 4; i++) {
    const itemObj = 9 + i;
    const prev = i > 0 ? ` /Prev ${itemObj - 1} 0 R` : "";
    const next = i < 3 ? ` /Next ${itemObj + 1} 0 R` : "";
    body[itemObj] = enc(`<< /Title (${titles[i]}) /Parent 8 0 R${prev}${next} /Dest [${3 + i} 0 R /Fit] >>`);
  }

  const maxN = 16;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0].length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n], enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function topWindows(pid: number): number[] {
  const out: number[] = [];
  enumWindows((h) => {
    if (getWindowPid(h) === pid) {
      out.push(h);
    }
    return true;
  });
  return out;
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5716-bookmarks.pdf");
  writeFileSync(pdfPath, makeBookmarkedPdf());

  const proc = launchSumatra([pdfPath]);
  try {
    const frame = await waitForFrame(proc.pid);
    if (!frame) {
      throw new Error("SumatraPDF frame window didn't appear");
    }
    moveWindow(frame, 40, 20, 1000, 760);
    await sleep(1200);

    const tree = findChildByClass(frame, "SysTreeView32");
    if (!tree) {
      throw new Error("Bookmarks (TOC) tree not found - did the outline load?");
    }
    const sel0 = treeGetSelection(tree); // Chapter One (page 1)

    // open the palette in TOC ("*") mode, filter to Chapter Four, navigate to it
    const before = new Set(topWindows(proc.pid));
    sendCommand(frame, CmdCommandPaletteTOC);
    await sleep(700);
    const palette = topWindows(proc.pid).find((h) => !before.has(h) && findChildWindow(h, "Edit")) ?? 0;
    if (!palette) {
      throw new Error("Command Palette window didn't open");
    }
    const edit = findChildWindow(palette, "Edit");
    sendText(edit, "*Four"); // keep the "*" TOC prefix; filter to "Chapter Four"
    await sleep(400);
    await pressEnter(edit); // navigate to the selected (Chapter Four) bookmark
    await sleep(600);

    const sel1 = treeGetSelection(tree);
    if (sel1 === sel0) {
      throw new Error(
        "issue #5716: Bookmarks panel selection did not move after navigating via the Command Palette",
      );
    }
  } finally {
    proc.kill();
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
