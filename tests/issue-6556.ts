// #6556: printing showed form fields as last saved, not as filled in. Printing
// renders from a Clone() of the document, which re-read the file from disk and
// lost unsaved changes. Toggle a checkbox and check that the clone renders the
// page the same as the open document.
import { writeFileSync } from "node:fs";
import type { ControlClient } from "./control.ts";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util.ts";

function makePdf(): string {
  // big enough that its check mark is many pixels
  const on =
    "<< /Type /XObject /Subtype /Form /BBox [0 0 100 100] /Length 20 >>\nstream\n0 g 10 10 80 80 re f\nendstream";
  const off = "<< /Type /XObject /Subtype /Form /BBox [0 0 100 100] /Length 0 >>\nstream\n\nendstream";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Widget /FT /Btn /T (Check) /Rect [100 600 200 700] /F 4 /V /Off /AS /Off /AP << /N << /Off 5 0 R /Yes 6 0 R >> >> >>",
    off,
    on,
  ]);
}

async function cloneDiffer(client: ControlClient): Promise<number> {
  const [code, raw] = await client.request(ControlCommand.TestCloneRender, []);
  const m = /^OK differ=(\d+)/.exec(String(raw ?? ""));
  if (code !== 0 || !m) throw new Error(`issue-6556: TestCloneRender: ${raw}`);
  return +m[1]!;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6556.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const before = await cloneDiffer(client);
      if (before !== 0) throw new Error(`issue-6556: an unchanged document's clone renders differently (${before})`);

      const r = await client.toggleFormButton(1, 0);
      if (!r.toggled) throw new Error(`issue-6556: the checkbox did not toggle: ${r.before} -> ${r.after}`);
      await client.waitForRenderIdle();
      const after = await cloneDiffer(client);
      if (after !== 0) {
        throw new Error(`issue-6556: the print clone lost the unsaved checkbox (${after} pixels differ)`);
      }
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
