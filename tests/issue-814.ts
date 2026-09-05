// Regression test for issue #814: Toggle Page Boxes reports only the PDF
// page boxes that page actually declares (MediaBox is required; Crop / Bleed /
// Trim / Art are optional and per-page).

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";

function buildPdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /CropBox [36 36 576 756] ` +
      `/TrimBox [72 72 540 720] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
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

function parseBoxes(raw: string): { page: number; show: number; names: string[] } {
  const m = /OK page=(\d+) show=(\d+)(.*)$/.exec(raw.trim());
  if (!m) {
    throw new Error(`issue-814: could not parse: ${raw}`);
  }
  const rest = m[3] ?? "";
  const names = [...rest.matchAll(/\b(media|crop|bleed|trim|art)=/g)].map((x) => x[1]!);
  return { page: parseInt(m[1]!, 10), show: parseInt(m[2]!, 10), names };
}

async function queryBoxes(client: ControlClient, pageNo: number): Promise<ReturnType<typeof parseBoxes>> {
  const deadline = Date.now() + 20_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const res = await client.request(ControlCommand.TestPageBoxes, [pageNo]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      return parseBoxes(raw);
    }
    if (exitCode !== 2) {
      throw new Error(`issue-814: TestPageBoxes failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-814: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

function expectNames(got: string[], want: string[], label: string): void {
  const a = got.join(",");
  const b = want.join(",");
  if (a !== b) {
    throw new Error(`issue-814 ${label}: want boxes [${b}], got [${a}]`);
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-814.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();

      const p1 = await queryBoxes(client, 1);
      expectNames(p1.names, ["media", "crop", "trim"], "page 1");
      if (p1.show !== 0) {
        throw new Error(`issue-814: overlay should start off, show=${p1.show}`);
      }

      const p2 = await queryBoxes(client, 2);
      expectNames(p2.names, ["media"], "page 2 (MediaBox only)");

      await client.request(ControlCommand.TestInvokeCommand, ["CmdTogglePageBoxes"]);
      const after = await queryBoxes(client, 1);
      if (after.show !== 1) {
        throw new Error(`issue-814: CmdTogglePageBoxes did not turn the overlay on (show=${after.show})`);
      }
      expectNames(after.names, ["media", "crop", "trim"], "page 1 after toggle");
    },
    [pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
