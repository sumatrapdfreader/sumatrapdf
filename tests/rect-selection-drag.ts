// Regression test: a rectangular (Ctrl+drag) selection can be moved / resized
// even though it sits over text.
//
// A rectangle is normally drawn over a text region, and the "clicking already
// selected text starts a drag-out" check in OnMouseLeftButtonDown ran first and
// claimed every press inside the rectangle, so it could never be grabbed. The
// rectangle hit-test now comes first.
//
// Drives the real app via -dbg-control: builds a rectangular selection around a
// word and presses the left button in the middle of it (over text), then checks
// the canvas started a selection edit rather than a text drag-out.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WORD = "grabme";

// one page with a line of text in the middle, big enough that a rectangle drawn
// around the word stays on the page
function buildPdf(): Buffer {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Kids [4 0 R] /Count 1 >>";
  objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
  const content = `BT /F1 24 Tf 200 400 Td (${WORD} in the middle) Tj ET`;
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
    const res = await client.request(ControlCommand.TestRectSelectionDrag, [WORD]);
    const exitCode = res[0] as number;
    const raw = ((res[1] as string) ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`rect-selection-drag: ${raw}`);
      }
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`rect-selection-drag: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("rect-selection-drag.pdf");
  writeFileSync(pdfPath, buildPdf());

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);
  console.log(`rect-selection-drag: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
