// The "Unsaved changes" prompt (a TaskDialog) can fail to show. orig took a
// failure as Discard and closed the tab, dropping the changes; it must keep
// the document open instead, as Cancel does.
//
// orig only: ng's prompt is drawn by gpui, not TaskDialogIndirect.
// Run: bun tests/save-prompt-fails.ts [--no-build]

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

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP save-prompt-fails: orig only");
    return;
  }
  const dir = tmpPath("save-prompt-fails");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "form.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const r = await client.toggleFormButton(1, 0);
    if (!r.toggled) {
      throw new Error(`save-prompt-fails: form button didn't toggle: ${JSON.stringify(r)}`);
    }

    await client.request(ControlCommand.TestFailSavePrompt, []);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdClose"]);
    await sleep(500);
    const path = await tabPath(client);
    if (!path.includes("form.pdf")) {
      throw new Error(`save-prompt-fails: the document was closed when the prompt failed (tab: ${path.trim()})`);
    }
  } finally {
    // the unsaved change would prompt again on a normal quit
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("save-prompt-fails: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
