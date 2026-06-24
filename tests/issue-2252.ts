// Regression test for issue #2252: page info overlay stale after reload.
//
// When a document is reloaded with a different page count, the page-info
// overlay should refresh via UpdateUiForCurrentTab.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function buildPdf(pageCount: number): Buffer {
  const pageKids = Array.from({ length: pageCount }, (_, i) => `${3 + i} 0 R`).join(" ");
  const objs: string[] = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    `<< /Type /Pages /Kids [${pageKids}] /Count ${pageCount} >>`,
  ];
  for (let i = 0; i < pageCount; i++) {
    const contents = `BT /F1 18 Tf 72 720 Td (Page ${i + 1}) Tj ET`;
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 ${3 + pageCount} 0 R >> >> /Contents ${4 + i} 0 R >>`,
    );
    objs.push(`<< /Length ${contents.length} >>\nstream\n${contents}\nendstream`);
  }
  objs.push("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");

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

async function requestWithRetry(client: ControlClient, twoPages: string, onePage: string): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestPageInfoOverlay, [twoPages, onePage]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-2252: page info overlay stale after reload: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-2252: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const twoPages = tmpPath("issue-2252-two.pdf");
  const onePage = tmpPath("issue-2252-one.pdf");
  writeFileSync(twoPages, buildPdf(2));
  writeFileSync(onePage, buildPdf(1));

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client, twoPages, onePage), [twoPages]);
  console.log(`issue-2252: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}