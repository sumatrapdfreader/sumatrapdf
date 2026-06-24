// Regression test: selecting a match from the Find floating results list.
//
// Picking a match (GoToFindMatch) used to call SetLastResult() before
// ShowSearchResult(). SetLastResult()->SetText() clears textSearch->result
// whenever the matched text differs from the typed search text (e.g. a
// case-insensitive find where typing "the" matches "The" in the document), so
// ShowSearchResult() then ran with an empty result (len == 0), tripped a
// ReportIf assert and failed to navigate to the match.
//
// Since issue #5737, find no longer sets a text selection (all matches are
// highlighted by PaintAllFindMatches), so this verifies *navigation* instead:
// the capitalized match lives on a later page, and after GoToFindMatch we check
// the app recorded it as the current find position and scrolled it into view.
//
// Drives the real app via -dbg-control: loads a multi-page PDF with the
// mixed-case target on the last page, primes the search with the lowercase
// typed text, then selects the capitalized match.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// filler pages deliberately avoid the substring "The" so the search only finds
// the target on the last page
const PAGES = ["Filler page one", "Filler page two", "The Quick Brown Fox"];
const WORD = "The"; // capitalized text in the document (on the last page)
const TYPED = "the"; // lowercase text the user typed (case-insensitive find)

// build a multi-page PDF, one text line per page. Objects: 1=Catalog, 2=Pages,
// 3=Font, then a Page + Contents object per page.
function buildPdf(pageLines: string[]): Buffer {
  const fontNum = 3;
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[fontNum] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const kids: number[] = [];
  let objNum = 4;
  for (const line of pageLines) {
    const pageNum = objNum++;
    const contentNum = objNum++;
    kids.push(pageNum);
    const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
    objs[pageNum] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontNum} 0 R >> >> /Contents ${contentNum} 0 R >>`;
    objs[contentNum] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objs[2] = `<< /Type /Pages /Kids [${kids.map((k) => `${k} 0 R`).join(" ")}] /Count ${pageLines.length} >>`;
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

async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestGoToFindMatch, [WORD, TYPED]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`find-match-select: GoToFindMatch failed to select match: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`find-match-select: document never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-find-match-select.pdf");
  writeFileSync(pdfPath, buildPdf(PAGES));

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);
  console.log(`find-match-select: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
