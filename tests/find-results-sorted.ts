// Regression test: the floating find window lists results in document order.
//
// The match-count scan starts at the page that is current when the search
// begins and wraps around, so it *finds* matches out of order. With a match on
// pages 2, 5 and 8 and the view on page 5, the scan produces 5, 8, 2 and the
// results list used to show them in that order. They must be sorted by
// (page, glyph) before being installed, so the list reads 2, 5, 8.
//
// Drives the real app via -dbg-control: builds a PDF with the search term on a
// few known pages, goes to a middle page, runs the search and asks for the
// order of win->findMatches (which is exactly what the list renders).

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WORD = "needle";
const MATCH_PAGES = [2, 5, 8];
const START_PAGE = 5; // in the middle of the matches, so the scan has to wrap
const PAGE_COUNT = 10;

// build a PAGE_COUNT-page PDF, one text line per page; only MATCH_PAGES contain
// WORD. Objects: 1=Catalog, 2=Pages, 3=Font, then a Page + Contents per page.
function buildPdf(): Buffer {
  const fontNum = 3;
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[fontNum] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const kids: number[] = [];
  let objNum = 4;
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const line = MATCH_PAGES.includes(page) ? `page ${page} has a ${WORD} on it` : `page ${page} filler`;
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontNum} 0 R >> >> /Contents ${contentNum} 0 R >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${PAGE_COUNT} >>`;
  const maxN = objNum - 1;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i <= maxN; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

// the scan is async: the first request kicks it off, later ones report NOTREADY
// until it finishes
async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFindResultsOrder, [WORD, START_PAGE]);
    const exitCode = res[0] as number;
    const raw = ((res[1] as string) ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`find-results-sorted: ${raw}`);
      }
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`find-results-sorted: search never finished: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 200));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("find-results-sorted.pdf");
  writeFileSync(pdfPath, buildPdf());

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);

  const m = /pages=([\d,]*)/.exec(result);
  if (!m) {
    throw new Error(`find-results-sorted: no page list in: ${result}`);
  }
  const pages = m[1].length === 0 ? [] : m[1].split(",").map((s) => parseInt(s, 10));
  const want = MATCH_PAGES.join(",");
  if (pages.join(",") !== want) {
    throw new Error(`find-results-sorted: results are ${pages.join(",")}, want ${want} (document order)`);
  }
  console.log(`find-results-sorted: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
