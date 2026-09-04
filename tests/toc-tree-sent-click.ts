// A synchronous cross-process SendMessage(WM_LBUTTONDOWN) on a bookmarks-tree
// row used to never return: the tree view had drag-and-drop enabled, so
// comctl32 ran its modal drag-detect loop waiting for a button-up that the
// blocked sender could not deliver. TreeView now sets TVS_DISABLEDRAGDROP
// (nothing handles TVN_BEGINDRAG), so the click returns and navigates.
//
// Run: bun tests/toc-tree-sent-click.ts [--no-build]

import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { findVisibleChildWindow, sleep, treeGetItemHeight, treeGetSelection } from "./winapi.ts";
import { runStandalone, tmpPath } from "./util.ts";

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

const CLICK_TIMEOUT_MS = 5000;

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("toc-tree-sent-click.pdf");
  writeFileSync(pdfPath, makeBookmarkedPdf());

  const { proc, client, frame } = await launchControlled([pdfPath]);
  let child: Bun.Subprocess | null = null;
  try {
    await client.waitForRenderIdle();
    const tree = findVisibleChildWindow(frame, "SysTreeView32");
    if (!tree) {
      throw new Error("toc-tree-sent-click: bookmarks tree not found");
    }
    const sel0 = treeGetSelection(tree);
    const h = treeGetItemHeight(tree);
    const rowNo = 3; // "Chapter Three" -> page 3
    const y = Math.floor((rowNo - 1) * h + h / 2);
    const childScript = join(import.meta.dir, "toc-tree-sent-click.child.ts");
    child = Bun.spawn(["bun", childScript, String(tree), "60", String(y)], { stdout: "ignore", stderr: "ignore" });
    const t0 = Date.now();
    while (child.exitCode == null && Date.now() - t0 < CLICK_TIMEOUT_MS) {
      await sleep(50);
    }
    if (child.exitCode == null) {
      child.kill();
      throw new Error(
        `toc-tree-sent-click: SendMessage(WM_LBUTTONDOWN) on a bookmarks-tree row did not return within ${CLICK_TIMEOUT_MS} ms (drag-detect loop?)`,
      );
    }
    await client.waitForRenderIdle();
    const sel1 = treeGetSelection(tree);
    const info = await client.chapterInfo();
    if (sel1 === sel0 || info.page !== rowNo) {
      throw new Error(
        `toc-tree-sent-click: click did not navigate: selection ${sel0} -> ${sel1}, page=${info.page} (expected ${rowNo})`,
      );
    }
  } finally {
    if (child && child.exitCode == null) {
      child.kill();
    }
    try {
      await client.quit();
    } catch {
      /* process already gone */
    }
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
