// Test for issue #6492: the Page Info tip (CmdTogglePageInfo) shows the percent
// read and, in full screen, starts with the file name.
//
// Generates a 10-page PDF, shows the tip and reads its text after turning pages
// and toggling full screen.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const kPageCount = 10;
const kFileName = "issue-6492.pdf";
const kSep = " · ";

function buildPdf(): Uint8Array {
  const kids: string[] = [];
  const objs: string[] = ["<< /Type /Catalog /Pages 2 0 R >>", ""];
  for (let i = 0; i < kPageCount; i++) {
    kids.push(`${3 + i} 0 R`);
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 300] >>");
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

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

// commands are posted: poll until the tip's text starts with want (the zoom follows)
async function expectTip(client: ControlClient, what: string, want: string): Promise<void> {
  let got = "";
  for (let i = 0; i < 50; i++) {
    const [, raw] = await client.request(ControlCommand.TestPageInfoText, []);
    got = String(raw ?? "")
      .trim()
      .replace(/^OK ?/, "");
    if (got.startsWith(want + kSep)) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-6492: ${what}: tip is "${got}", want it to start with "${want}"`);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath(kFileName);
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();

      await invoke(client, "CmdTogglePageInfo");
      await expectTip(client, "page 1", "Page: 1 / 10 (10%)");
      await invoke(client, "CmdGoToNextPage 2");
      await expectTip(client, "page 3", "Page: 3 / 10 (30%)");

      await invoke(client, "CmdToggleFullscreen");
      await expectTip(client, "full screen", `${kFileName}${kSep}Page: 3 / 10 (30%)`);
      await invoke(client, "CmdToggleFullscreen");
      await expectTip(client, "left full screen", "Page: 3 / 10 (30%)");

      await invoke(client, "CmdGoToLastPage");
      await expectTip(client, "last page", "Page: 10 / 10 (100%)");
    },
    ["-zoom", "100", pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
