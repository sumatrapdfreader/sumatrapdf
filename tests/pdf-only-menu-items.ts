// The canvas context menu's Document submenu offered the PDF-only commands
// (Show PDF Info, Encrypt PDF, Compress PDF, Extract/Delete Pages From PDF, ...)
// for every document the mupdf engine renders, so an epub got the whole list.
// The gate used CouldBePDFDoc(), which is true for epub/mobi/fb2/xps/svg too;
// it now asks the engine whether there is really a pdf_document behind the tab.

import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

const kPdfCmds = ["CmdPdShowInfo", "CmdPdfCompress", "CmdPdfExtractPages", "CmdPdfEncrypt"];

async function vis(client: { request: Function }, cmd: string): Promise<string> {
  const res = await client.request(ControlCommand.TestCommandVisibility, [cmd, "menu"]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`pdf-only-menu-items: ${cmd}: ${raw.trim()}`);
  }
  const m = /vis=(\w+)/.exec(raw);
  return m?.[1] ?? "";
}

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      for (const cmd of kPdfCmds) {
        const v = await vis(client, cmd);
        if (v !== "hide") {
          throw new Error(`pdf-only-menu-items: epub still shows ${cmd} vis=${v}`);
        }
      }
    },
    ["tests/issue-5846.epub"],
  );

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const shown = [];
      for (const cmd of kPdfCmds) {
        const v = await vis(client, cmd);
        if (v === "show") {
          shown.push(cmd);
        }
      }
      if (shown.length === 0) {
        throw new Error("pdf-only-menu-items: pdf hid every PDF command");
      }
    },
    ["tests/issue-1189.pdf"],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
