// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6055
//
// Opening a PDF with `-search <term>` then showing the floating Find window
// used to leave the search box empty and the results list as blank rows
// (snippets were never built because the count ran before the window existed).
//
// Run:  bun tests/issue-6055.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const WORD = "needle";

function makePdf(): Buffer {
  const line = `page 1 has a ${WORD} here and another ${WORD} too`;
  const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
  const font = `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>`;
  const page =
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
    `/Resources << /Font << /F1 3 0 R >> >> /Contents 5 0 R >>`;
  const stream = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  const objs = [`<< /Type /Catalog /Pages 2 0 R >>`, `<< /Type /Pages /Kids [4 0 R] /Count 1 >>`, font, page, stream];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFindWindowContents);
    const exitCode = res[0] as number;
    const raw = ((res[1] as string) ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-6055: ${raw}`);
      }
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6055: find window never filled: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 200));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6055.pdf");
  writeFileSync(pdfPath, makePdf());

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), ["-search", WORD, pdfPath]);

  const m = /OK term=(\S+) n=(\d+) snippets=(\d+) first=(.*)$/.exec(result);
  if (!m) {
    throw new Error(`issue-6055: unexpected result: ${result}`);
  }
  const term = m[1]!;
  const n = parseInt(m[2]!, 10);
  const nSnippets = parseInt(m[3]!, 10);
  const first = m[4]!;
  if (term !== WORD) {
    throw new Error(`issue-6055: find box term is '${term}', want '${WORD}'`);
  }
  if (n < 2) {
    throw new Error(`issue-6055: expected at least 2 matches, got n=${n}`);
  }
  if (nSnippets !== n) {
    throw new Error(`issue-6055: snippets=${nSnippets} want ${n} (empty result rows)`);
  }
  if (!first.toLowerCase().includes(WORD)) {
    throw new Error(`issue-6055: first snippet does not contain '${WORD}': ${first}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
