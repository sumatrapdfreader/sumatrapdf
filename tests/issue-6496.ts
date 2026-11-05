// Test for issue #6496: CmdShowSelectedText shows the selection's text as the
// document has it, cut at 1000 characters.
//
// Generates a 2-page PDF: page 1 has two short lines, page 2 more than 1000
// characters. Selects each page (CmdSelectCurrentPage) and reads the notification.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const kShortLines = ["first line", "second line"];
const kLongLine = "0123456789".repeat(5);
const kLongLineCount = 30;

function pageContent(lines: string[]): string {
  return lines.map((l, i) => `BT /F1 10 Tf 20 ${760 - i * 14} Td (${l}) Tj ET`).join("\n");
}

function buildPdf(): Uint8Array {
  const pages = [pageContent(kShortLines), pageContent(Array(kLongLineCount).fill(kLongLine))];
  const objs: string[] = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Courier >>",
  ];
  const kids: string[] = [];
  for (const content of pages) {
    const pageObj = objs.length + 1;
    kids.push(`${pageObj} 0 R`);
    objs.push(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${pageObj + 1} 0 R >>`,
    );
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${pages.length} >>`;
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

async function shownText(client: ControlClient): Promise<string> {
  const [, raw] = await client.request(ControlCommand.TestSelectedText, []);
  return String(raw ?? "").replace(/^OK ?/, "");
}

// commands are posted: poll until ok(text)
async function waitText(client: ControlClient, what: string, ok: (s: string) => boolean): Promise<string> {
  let s = "";
  for (let i = 0; i < 50; i++) {
    s = await shownText(client);
    if (ok(s)) {
      return s;
    }
    await sleep(100);
  }
  throw new Error(`issue-6496: ${what}: shown text is ${JSON.stringify(s)}`);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6496.pdf");
  writeFileSync(pdfPath, buildPdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      await client.setNotificationsEnabled(true);

      // nothing selected: nothing to show
      await invoke(client, "CmdShowSelectedText");
      await sleep(300);
      if ((await shownText(client)) !== "") {
        throw new Error("issue-6496: showed text without a selection");
      }

      await invoke(client, "CmdSelectCurrentPage");
      await invoke(client, "CmdShowSelectedText");
      // lines of one paragraph come out joined, as Copy gives them
      const words = (s: string) => s.trim().split(/\s+/).join(" ");
      await waitText(client, "page 1", (s) => words(s) === kShortLines.join(" "));

      // more than kSelectedTextMaxChars: cut, with a note
      await invoke(client, "CmdGoToNextPage");
      await sleep(300);
      await invoke(client, "CmdSelectCurrentPage");
      await invoke(client, "CmdShowSelectedText");
      const s = await waitText(client, "page 2", (s) => s.includes("the first 1000 of"));
      if (!s.startsWith(kLongLine) || s.includes("first line")) {
        throw new Error(`issue-6496: page 2: ${JSON.stringify(s.slice(0, 80))}`);
      }
    },
    [pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
