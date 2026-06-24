// Regression test for issue #5064: horizontal scroll when following internal PDF links.
//
// A wide page with a link destination at x=400 should scroll the viewport
// horizontally after the link is followed (viewport was pinned to the left).

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function buildWidePdfWithLink(): Buffer {
  const content = "BT /F1 18 Tf 72 700 Td (Click link) Tj ET";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 2400 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R /Annots [6 0 R] >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    "<< /Type /Annot /Subtype /Link /Rect [72 680 180 710] /Border [0 0 0] /Dest [3 0 R /XYZ 400 700 null] >>",
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

async function requestWithRetry(client: ControlClient, minDelta: number): Promise<string> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestScrollToLink, [minDelta]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5064: link did not scroll horizontally: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5064: document never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5064.pdf");
  writeFileSync(pdfPath, buildWidePdfWithLink());

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client, 50), [pdfPath]);
  console.log(`issue-5064: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}