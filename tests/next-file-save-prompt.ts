// Opening the next file in the folder replaces the current tab's document, so
// unsaved changes must get the save prompt first. ng loaded the next file
// right away and dropped them.
//
// ng only: orig's prompt is a modal TASKDIALOG the control pipe can't answer.
// Run: bun tests/next-file-save-prompt.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

// one checkbox: toggling it is an unsaved change
function makePdf(): string {
  const on = "<< /Type /XObject /Subtype /Form /BBox [0 0 12 12] /Length 16 >>\nstream\n0 g 2 2 8 8 re f\nendstream";
  const off = "<< /Type /XObject /Subtype /Form /BBox [0 0 12 12] /Length 0 >>\nstream\n\nendstream";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Widget /FT /Btn /T (Check) /Rect [50 660 62 672] /F 4 /V /Off /AS /Off /AP << /N << /Off 5 0 R /Yes 6 0 R >> >> >>",
    off,
    on,
  ]);
}

async function tabPath(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return String(res[1] ?? "");
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP next-file-save-prompt: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("next-file-save-prompt");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "aaa.pdf"), makePdf(), "latin1");
  writeFileSync(join(dir, "bbb.pdf"), makePdf(), "latin1");

  const { proc, client } = await launchControlled([join(dir, "aaa.pdf")]);
  try {
    await client.waitForRenderIdle();
    const r = await client.toggleFormButton(1, 0);
    if (!r.toggled) {
      throw new Error(`next-file-save-prompt: form button didn't toggle: ${JSON.stringify(r)}`);
    }

    await client.request(ControlCommand.TestInvokeCommand, ["CmdOpenNextFileInFolder"]);
    const deadline = Date.now() + 5000;
    for (;;) {
      const path = await tabPath(client);
      if (!path.includes("aaa.pdf")) {
        throw new Error(`next-file-save-prompt: replaced the document without asking (${path.trim()})`);
      }
      if (/dialog=1/.test(await uiState(client))) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`next-file-save-prompt: no save prompt: ${await uiState(client)}`);
      }
      await sleep(50);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("next-file-save-prompt: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
