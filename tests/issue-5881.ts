// Regression test for issue #5881: clicking empty space clears the text
// selection.
//
// #5737 made find-match highlights independent of the text selection, which
// meant ClearSearchResult() stopped clearing the selection. The "this was a
// click, not a drag" path in OnMouseLeftButtonUp called exactly that to drop
// the selection, so clicking in the blank margin no longer deselected -- you
// had to click another word, or press Esc.
//
// Drives the real app via -dbg-control: selects a word, then sends a real left
// click (down + up, no movement) at a point on the page with no text under it,
// so the whole canvas mouse path runs, and checks nothing is selected after.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WORD = "selectme";

// a one-page PDF with a short line of text near the top, so most of the page is
// blank space to click in
function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Kids [4 0 R] /Count 1 >>";
  objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const content = `BT /F1 24 Tf 72 720 Td (${WORD} here) Tj ET`;
  objs[4] =
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
    `/Resources << /Font << /F1 3 0 R >> >> /Contents 5 0 R >>`;
  objs[5] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i <= 5; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 6\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestClickClearsSelection, [WORD]);
    const exitCode = res[0] as number;
    const raw = ((res[1] as string) ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5881: ${raw}`);
      }
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5881: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5881.pdf");
  writeFileSync(pdfPath, buildPdf());

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);
  console.log(`issue-5881: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
