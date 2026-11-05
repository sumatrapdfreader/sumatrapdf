// Test for issue #6487: a two-page view that moves one page at a time.
//
// Generates a 5-page PDF, switches to Facing, turns on CmdToggleFacingStepOne
// and checks the pages shown as next / previous / last / first page step:
// 1-2, 2-3, 3-4, 4-5 (and no further), and Book View's lone cover.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const kPageCount = 5;

function buildPdf(): Uint8Array {
  const kids = [];
  const objs: string[] = ["<< /Type /Catalog /Pages 2 0 R >>", ""];
  for (let i = 0; i < kPageCount; i++) {
    const pageObj = 3 + i * 2;
    kids.push(`${pageObj} 0 R`);
    const content = `0 0 0 rg 20 20 ${10 + i * 10} 60 re f`;
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 300] /Contents ${pageObj + 1} 0 R >>`);
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${kPageCount} >>`;
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

// "OK mode=facing step=1 current=2 shown=2,3" => { mode, step, shown: "2,3" }
async function pages(client: ControlClient): Promise<string> {
  const [, raw] = await client.request(ControlCommand.TestDisplayMode, ["pages"]);
  const s = String(raw ?? "").trim();
  if (!s.startsWith("OK ")) {
    throw new Error(`issue-6487: TestDisplayMode pages: ${s}`);
  }
  return s.slice(3);
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

// the command is posted: poll until the view shows what we want
async function expectPages(client: ControlClient, what: string, want: string): Promise<void> {
  let got = "";
  for (let i = 0; i < 50; i++) {
    got = await pages(client);
    if (got === want) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-6487: ${what}: got "${got}", want "${want}"`);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6487.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const facing = (step: number, cur: number, shown: string) =>
        `mode=facing step=${step} current=${cur} shown=${shown}`;

      // starts in continuous view
      await invoke(client, "CmdFacingView");
      await invoke(client, "CmdToggleContinuousView");
      await expectPages(client, "facing", facing(0, 1, "1,2"));
      await invoke(client, "CmdGoToNextPage");
      await expectPages(client, "facing next page", facing(0, 3, "3,4"));
      await invoke(client, "CmdGoToFirstPage");
      await expectPages(client, "facing first page", facing(0, 1, "1,2"));

      await invoke(client, "CmdToggleFacingStepOne");
      await expectPages(client, "step on", facing(1, 1, "1,2"));
      await invoke(client, "CmdGoToNextPage");
      await expectPages(client, "step next", facing(1, 2, "2,3"));
      await invoke(client, "CmdGoToNextPage");
      await expectPages(client, "step next 2", facing(1, 3, "3,4"));
      await invoke(client, "CmdGoToPrevPage");
      await expectPages(client, "step prev", facing(1, 2, "2,3"));

      // the last two pages, and no further
      await invoke(client, "CmdGoToLastPage");
      await expectPages(client, "step last page", facing(1, 4, "4,5"));
      await invoke(client, "CmdGoToNextPage");
      await sleep(300);
      await expectPages(client, "step next at end", facing(1, 4, "4,5"));

      // turning it off goes back to the row of the start page
      await invoke(client, "CmdToggleFacingStepOne");
      await expectPages(client, "step off", facing(0, 3, "3,4"));

      // Book View: the cover is alone, then 2-3, 3-4
      await invoke(client, "CmdBookView");
      await invoke(client, "CmdToggleFacingStepOne");
      await invoke(client, "CmdGoToFirstPage");
      await expectPages(client, "book cover", "mode=book view step=1 current=1 shown=1");
      await invoke(client, "CmdGoToNextPage");
      await expectPages(client, "book next", "mode=book view step=1 current=2 shown=2,3");
      await invoke(client, "CmdGoToNextPage");
      await expectPages(client, "book next 2", "mode=book view step=1 current=3 shown=3,4");
      await invoke(client, "CmdGoToPrevPage");
      await invoke(client, "CmdGoToPrevPage");
      await expectPages(client, "book back to cover", "mode=book view step=1 current=1 shown=1");

      // turning it on in single page view switches to Facing
      await invoke(client, "CmdToggleFacingStepOne");
      await invoke(client, "CmdSinglePageView");
      await invoke(client, "CmdToggleFacingStepOne");
      await expectPages(client, "on in single page", facing(1, 1, "1,2"));
    },
    [pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
