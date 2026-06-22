// Regression test for issue #5718.
//
// When text is selected and the user opens the context menu over text and picks
// "Copy Selection", the selected text must be copied. The bug was that building
// the context menu called ReadAloudCanReadFromCursor(), which mutated the live
// TextSelection's start glyph, so the copied text ran from the old selection end
// to the cursor instead of the actual selection.
//
// This drives the real app via -dbg-control: it loads a one-line PDF, selects
// word1..word2 on the live document, then runs the same read-only cursor check
// the context menu performs and verifies the selection text is unchanged.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const LINE = "alpha beta gamma delta epsilon";
const WORD1 = "alpha";
const WORD2 = "epsilon";
const CURSOR_WORD = "gamma"; // distinct from the selection start, so the bug is observable

function buildPdf(line: string): Buffer {
  const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function requestWithRetry(client: ControlClient): Promise<string> {
  // the document is loaded asynchronously after the app starts; retry until the
  // window/document is ready (the command returns "NOTREADY ..." until then)
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestContextMenuSelection, [WORD1, WORD2, CURSOR_WORD]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5718: context menu corrupted selection: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5718: document never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5718.pdf");
  writeFileSync(pdfPath, buildPdf(LINE));

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);
  console.log(`issue-5718: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
