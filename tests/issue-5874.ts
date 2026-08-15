// Regression test for issue #5874 (comment about a term with a space):
// a search started from a page that has no match at or after it reported
// zero matches.
//
// The match-count scan starts at the current page and wraps around to cover
// pages 1..startPage-1. The wrap only happened from inside the "next match"
// loop, so when the very first FindFirst(startPage) came up empty the loop was
// never entered and the scan ended with no matches at all: no "n / m", no
// highlights on the page, and an empty results list -- even though the document
// did contain the term on an earlier page. Pressing Find Next still navigated
// to the match, which is what made it look like "it goes to the result but
// doesn't show it".
//
// Drives the real app via -dbg-control: builds a PDF with the term only on an
// early page, runs the search from a much later page, and checks the match is
// found.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WORD = "needle";
const MATCH_PAGE = 2;
// far enough past the match that the incremental find bails out before
// reaching it (leaving the count scan to start here), as it does in the report
const START_PAGE = 15;
const PAGE_COUNT = 20;

// build a PAGE_COUNT-page PDF; only MATCH_PAGE contains WORD.
// Objects: 1=Catalog, 2=Pages, 3=Font, then a Page + Contents per page.
function buildPdf(): Buffer {
  const fontNum = 3;
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[fontNum] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const kids: number[] = [];
  let objNum = 4;
  for (let page = 1; page <= PAGE_COUNT; page++) {
    const line = page === MATCH_PAGE ? `page ${page} has a ${WORD} on it` : `page ${page} filler`;
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
        throw new Error(`issue-5874: ${raw}`);
      }
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5874: search never finished: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 200));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5874.pdf");
  writeFileSync(pdfPath, buildPdf());

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);

  const m = /pages=([\d,]*)/.exec(result);
  if (!m) {
    throw new Error(`issue-5874: no page list in: ${result}`);
  }
  const pages = m[1].length === 0 ? [] : m[1].split(",").map((s) => parseInt(s, 10));
  if (pages.length !== 1 || pages[0] !== MATCH_PAGE) {
    throw new Error(
      `issue-5874: searching from page ${START_PAGE} found matches on [${pages.join(",")}], ` +
        `want [${MATCH_PAGE}] (the scan must wrap around to earlier pages)`,
    );
  }
  console.log(`issue-5874: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
