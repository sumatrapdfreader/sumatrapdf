// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5694
//
// Find can be limited to a page range. "ALPHA" is on pages 1, 2 and 3 of the
// fixture; restricting to 2-2 must return only page 2.
//
// Run:  bun tests/issue-5694.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(): Buffer {
  const font = `<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>`;
  const page = (contentsObj: number) =>
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${contentsObj} 0 R >>`;
  const stream = (s: string) => `<< /Length ${s.length} >>\nstream\n${s}\nendstream`;
  const c1 = stream("BT /F1 24 Tf 72 720 Td (ALPHA one) Tj ET");
  const c2 = stream("BT /F1 24 Tf 72 720 Td (BETA ALPHA two) Tj ET");
  const c3 = stream("BT /F1 24 Tf 72 720 Td (GAMMA ALPHA three) Tj ET");
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [4 0 R 6 0 R 8 0 R] /Count 3 >>`,
    font,
    page(5),
    c1,
    page(7),
    c2,
    page(9),
    c3,
  ];
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

function pagesOf(raw: string): number[] {
  const out: number[] = [];
  for (const line of raw.split("\n")) {
    const m = /^page=(\d+)\s*$/.exec(line.trim());
    if (m) {
      out.push(parseInt(m[1]!, 10));
    }
  }
  return out;
}

async function search(
  client: ControlClient,
  pdf: string,
  first: number,
  last: number,
  spec?: string,
): Promise<number[]> {
  const args: Array<number | string> = [pdf, "ALPHA", first, last];
  if (spec !== undefined) {
    args.push(spec);
  }
  const [exitCode, raw] = await client.request(ControlCommand.TestFindPageRange, args);
  if (exitCode !== 0) {
    throw new Error(
      `issue-5694: search first=${first} last=${last} spec=${spec ?? ""} failed: ${String(raw ?? "").trim()}`,
    );
  }
  return pagesOf(String(raw ?? ""));
}

function expectPages(got: number[], want: number[], what: string): void {
  if (got.length !== want.length || got.some((p, i) => p !== want[i])) {
    throw new Error(`issue-5694 ${what}: want [${want.join(",")}], got [${got.join(",")}]`);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5694.pdf");
  writeFileSync(pdf, makePdf());

  await withControlledSumatra(EXE, async (client) => {
    expectPages(await search(client, pdf, 0, 0), [1, 2, 3], "all pages");
    expectPages(await search(client, pdf, 2, 2), [2], "only page 2");
    expectPages(await search(client, pdf, 2, 3), [2, 3], "pages 2-3");
    expectPages(await search(client, pdf, 1, 1), [1], "only page 1");
    expectPages(await search(client, pdf, 0, 0, "1,3"), [1, 3], "pages 1 and 3");
    expectPages(await search(client, pdf, 0, 0, "2-"), [2, 3], "page 2 through end");
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
