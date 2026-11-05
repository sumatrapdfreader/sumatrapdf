// Test for issue #6489: CmdCountWords counts the words of the document, or of
// the selection.
//
// Generates a 2-page PDF with known text, counts the whole document (counted on
// a thread, so the test polls the notification), then Select All and counts the
// selection.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

// 7 words, 35 characters without spaces
const kPages = ["Hello world from SumatraPDF", "one two three"];
const kResult = "Words: 7, characters (no spaces): 35";

function buildPdf(): Uint8Array {
  const kids: string[] = [];
  const objs: string[] = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ];
  for (const text of kPages) {
    const pageObj = objs.length + 1;
    kids.push(`${pageObj} 0 R`);
    const content = `BT /F1 24 Tf 40 700 Td (${text}) Tj ET`;
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${pageObj + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${kPages.length} >>`;
  let pdf = "%PDF-1.4\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = pdf.length;
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return new TextEncoder().encode(pdf);
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

// the command is posted and the document counted on a thread: poll the notification
async function expectCount(client: ControlClient, want: string): Promise<void> {
  let got = "";
  for (let i = 0; i < 50; i++) {
    const [, raw] = await client.request(ControlCommand.TestWordCount, []);
    got = String(raw ?? "").trim();
    if (got === `OK ${want}`) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-6489: got "${got}", want "OK ${want}"`);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6489.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();

      await invoke(client, "CmdCountWords");
      await expectCount(client, kResult);

      await invoke(client, "CmdSelectAll");
      await invoke(client, "CmdCountWords");
      await expectCount(client, `Selection: ${kResult}`);
    },
    [pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
