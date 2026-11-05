// Test for issue #6490: CmdScrollLeft / CmdScrollRight take a count, like
// CmdScrollUp / CmdScrollDown ("CmdScrollRight 3" scrolls 3 steps).
//
// Generates a wide page, zooms in so it scrolls horizontally, and compares the
// horizontal scroll position after one step and after counted steps.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

function buildPdf(): Uint8Array {
  const content = "0 0 1 rg 0 0 3000 300 re f";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 3000 300] /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
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

// "OK page=1 x=12.5 y=0" => 12.5
async function scrollX(client: ControlClient): Promise<number> {
  const [, raw] = await client.request(ControlCommand.TestScrollState, []);
  const s = String(raw ?? "").trim();
  const m = /x=(-?[\d.e+-]+)/.exec(s);
  if (!s.startsWith("OK ") || !m) {
    throw new Error(`issue-6490: TestScrollState: ${s}`);
  }
  return Number(m[1]);
}

// the command is posted: poll until the position is near want
async function scrollAndExpect(client: ControlClient, cmd: string, want: number): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  let got = 0;
  for (let i = 0; i < 50; i++) {
    got = await scrollX(client);
    if (Math.abs(got - want) < 1) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-6490: after '${cmd}' x=${got}, want ${want}`);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6490.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const x0 = await scrollX(client);

      // one step, to learn its size
      await client.request(ControlCommand.TestInvokeCommand, ["CmdScrollRight"]);
      let x1 = x0;
      for (let i = 0; i < 50 && x1 === x0; i++) {
        await sleep(100);
        x1 = await scrollX(client);
      }
      const step = x1 - x0;
      if (step <= 0) {
        throw new Error(`issue-6490: CmdScrollRight didn't scroll (x=${x1})`);
      }

      await scrollAndExpect(client, "CmdScrollRight 3", x1 + 3 * step);
      await scrollAndExpect(client, "CmdScrollLeft 2", x1 + step);
    },
    ["-zoom", "400", pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
