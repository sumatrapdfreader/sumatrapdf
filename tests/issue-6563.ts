// #6563: picking a value in a combo box set the field's /V but left its /I
// (indices of the selected options) as it was, so viewers that go by /I, like
// Firefox, showed the old value in the saved file. /I must follow /V.
import { readFileSync, rmSync, writeFileSync } from "node:fs";
import type { ControlClient } from "./control.ts";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { sleep, waitForTopWindow } from "./winapi.ts";
import { FRAME_CLASS, sendCommand } from "./win-automation.ts";

const kComboFlag = 1 << 17;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] /DA (/Helv 0 Tf 0 g) >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    `<< /Type /Annot /Subtype /Widget /FT /Ch /Ff ${kComboFlag} /T (Choice) /Opt [(Izaberi) (Da) (Ne)] ` +
      "/V (Izaberi) /I [0] /Rect [100 600 300 630] /F 4 /DA (/Helv 12 Tf 0 g) /P 3 0 R >>",
  ]);
}

// the field's dictionary in the last revision that has it
function lastField(pdf: string): string {
  const objs = [...pdf.matchAll(/\d+ 0 obj\s*<<([\s\S]*?)endobj/g)].map((m) => m[1]!);
  const field = objs.filter((o) => /\/T\s*\(Choice\)/.test(o)).at(-1);
  if (!field) throw new Error("issue-6563: the saved PDF has no Choice field");
  return field;
}

async function waitSaved(path: string, before: string): Promise<string> {
  const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const now = readFileSync(path, "latin1");
    if (now !== before) return now;
    if (Date.now() > deadline) throw new Error("issue-6563: saving didn't change the file");
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6563.pdf");
  rmSync(pdf, { force: true });
  writeFileSync(pdf, makePdf(), "latin1");
  const original = readFileSync(pdf, "latin1");
  await withControlledSumatra(
    EXE,
    async (client: ControlClient, proc) => {
      await client.waitForRenderIdle();
      const [code, raw] = await client.request(ControlCommand.TestSetFormChoice, [1, 0, "Ne"]);
      if (code !== 0 || !/after='Ne'/.test(String(raw))) throw new Error(`issue-6563: TestSetFormChoice: ${raw}`);
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      sendCommand(frame, cmdId("CmdSaveAnnotations"));
      const saved = await waitSaved(pdf, original);
      const field = lastField(saved);
      if (!/\/V\s*\(Ne\)/.test(field)) throw new Error(`issue-6563: /V isn't Ne:\n${field}`);
      const i = /\/I\s*\[([^\]]*)\]/.exec(field);
      if (i && i[1]!.trim() !== "2") {
        throw new Error(`issue-6563: /I [${i[1]}] doesn't select Ne, the value:\n${field}`);
      }
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
