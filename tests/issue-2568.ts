// #2568: Explorer Space-bar preview (`-quicklook`) must load the file as a
// normal document so the overlay window is actually showing a page.
import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control";
import { EXE, runStandalone, tmpPath } from "./util";

function makePdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const stream = "BT /F1 24 Tf 72 700 Td (quicklook) Tj ET";
  const body: Record<number, Buffer> = {
    1: enc("<< /Type /Catalog /Pages 2 0 R >>"),
    2: enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    3: enc(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>`,
    ),
    4: enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>"),
    5: enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`),
  };
  const maxN = 5;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n]!, enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-2568.pdf");
  writeFileSync(pdf, makePdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
      const dump = String(res[1] ?? "");
      // no links is fine; engine-create / page-load must succeed
      if (dump.includes("engine-create-failed") || dump.includes("page-load-failed")) {
        throw new Error(`issue-2568: -quicklook did not load the PDF:\n${dump}`);
      }
    },
    ["-quicklook", pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
