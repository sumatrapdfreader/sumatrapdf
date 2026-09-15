// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6190
//
// Outline entries with /FitH top must land on their page. ScrollTo treated the
// page-relative top as an absolute document offset, so they jumped to page 1.
//
// Run:  bun tests/issue-6190.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 6 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Outlines /First 7 0 R /Last 11 0 R /Count 5 >>`,
    `<< /Title (Page3 FitH) /Parent 6 0 R /Next 8 0 R /A << /S /GoTo /D [5 0 R /FitH 600] >> >>`,
    `<< /Title (Page2 FitH) /Parent 6 0 R /Prev 7 0 R /Next 9 0 R /Dest [4 0 R /FitH 700] >>`,
    `<< /Title (Page3 FitH top) /Parent 6 0 R /Prev 8 0 R /Next 10 0 R /Dest [5 0 R /FitH 792] >>`,
    // near the bottom and below the page: must not land on the next page
    `<< /Title (Page2 FitH low) /Parent 6 0 R /Prev 9 0 R /Next 11 0 R /Dest [4 0 R /FitH 30] >>`,
    `<< /Title (Page1 FitH below) /Parent 6 0 R /Prev 10 0 R /Dest [3 0 R /FitH -200] >>`,
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

// TestTocNavigate scrolls to page 1 first, then follows the dest and reports
// "OK/FAIL dest=N expect=P landed=Q"
async function navigate(client: ControlClient, destNo: number): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestTocNavigate, [destNo]);
    const raw = String(res[1] ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      return raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6190: never ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const PDF = tmpPath("issue-6190-fith-dests.pdf");
  writeFileSync(PDF, makePdf());

  const results = await withControlledSumatra(
    EXE,
    async (client) => {
      const out: string[] = [];
      for (let d = 1; d <= 5; d++) {
        out.push(await navigate(client, d));
      }
      return out;
    },
    [PDF],
  );

  const failed = results.filter((line) => !line.startsWith("OK "));
  for (const line of results) {
    console.log(`issue-6190: ${line}`);
  }
  if (failed.length > 0) {
    throw new Error(`issue-6190: /FitH bookmarks landed on the wrong page:\n${failed.join("\n")}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
