// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/2799
// (and the related #3310 page-level outline destinations).
//
// Page-level outline entries (/Fit, /XYZ with null coords) must land on the
// destination page, not one page ahead. That failed when unspecified PDF
// coordinates were treated as user-space (0,0) (bottom of page) or when
// ScrollTo reused the previous page's vertical offset in continuous view.
//
// Builds a 3-page PDF with /Fit and /XYZ null destinations, opens it, scrolls
// away from page 1, then navigates each outline entry and checks CurrentPageNo.
//
// Run:  bun tests/issue-2799.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const PDF = tmpPath("issue-2799-page-dests.pdf");

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Outlines /First 7 0 R /Last 9 0 R /Count 3 >>`,
    `<< /Title (Page1 Fit) /Parent 6 0 R /Next 8 0 R /Dest [3 0 R /Fit] >>`,
    `<< /Title (Page2 Fit) /Parent 6 0 R /Prev 7 0 R /Next 9 0 R /Dest [4 0 R /Fit] >>`,
    `<< /Title (Page3 XYZ null) /Parent 6 0 R /Prev 8 0 R /Dest [5 0 R /XYZ null null null] >>`,
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
    pdf += off[i].toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function navigate(client: ControlClient, destNo: number): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestTocNavigate, [destNo]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-2799: dest ${destNo} failed: ${raw}`);
      }
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-2799: never ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  writeFileSync(PDF, makePdf());

  const results = await withControlledSumatra(
    EXE,
    async (client) => {
      const out: string[] = [];
      // dests 1,2,3 -> pages 1,2,3
      for (let d = 1; d <= 3; d++) {
        out.push(await navigate(client, d));
      }
      return out;
    },
    [PDF],
  );

  for (const line of results) {
    console.log(`issue-2799: ${line}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
