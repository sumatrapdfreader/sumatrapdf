// #6240: clicking a radio button in a journalled (undo-enabled) PDF failed with
// "Can't alter an object other than in an operation": the radio branch of
// ToggleFormButton wrote /V and /AS outside a MuPDF operation. The group here
// mirrors the reporter's form: NoToggleToOff, kids with distinct on-state names.
//
// Run: bun tests/issue-6240.ts [--no-build]

import { rmSync, writeFileSync } from "node:fs";
import { ControlClient, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util.ts";

const FF_RADIO_NO_TOGGLE_OFF = 16384 | 32768; // NoToggleToOff (bit 15) | Radio (bit 16)
const FF_CHECKBOX = 0;

function makePdf(): string {
  const on = "<< /Type /XObject /Subtype /Form /BBox [0 0 12 12] /Length 16 >>\nstream\n0 g 2 2 8 8 re f\nendstream";
  const off = "<< /Type /XObject /Subtype /Form /BBox [0 0 12 12] /Length 0 >>\nstream\n\nendstream";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R 7 0 R] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [5 0 R 6 0 R 7 0 R] >>",
    `<< /FT /Btn /Ff ${FF_RADIO_NO_TOGGLE_OFF} /T (Group) /V /Off /Kids [5 0 R 6 0 R] >>`,
    "<< /Type /Annot /Subtype /Widget /Parent 4 0 R /Rect [50 700 62 712] /F 4 /AS /Off /AP << /N << /Off 8 0 R /A 9 0 R >> >> >>",
    "<< /Type /Annot /Subtype /Widget /Parent 4 0 R /Rect [50 680 62 692] /F 4 /AS /Off /AP << /N << /Off 8 0 R /B 9 0 R >> >> >>",
    `<< /Type /Annot /Subtype /Widget /FT /Btn /Ff ${FF_CHECKBOX} /T (Check) /Rect [50 660 62 672] /F 4 /V /Off /AS /Off /AP << /N << /Off 8 0 R /Yes 9 0 R >> >> >>`,
    off,
    on,
  ];
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6240.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const saved = tmpPath("issue-6240-saved.pdf");
  rmSync(saved, { force: true });

  await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      await client.waitForRenderIdle();

      // radio: first click turns button 0 on, second click on button 1 moves
      // the selection, NoToggleToOff keeps a click on the selected one on
      let r = await client.toggleFormButton(1, 0);
      if (!r.toggled || r.after !== "A") {
        throw new Error(`issue-6240: radio 0 -> ${JSON.stringify(r)}, want after='A'`);
      }
      r = await client.toggleFormButton(1, 1);
      if (!r.toggled || r.after !== "B") {
        throw new Error(`issue-6240: radio 1 -> ${JSON.stringify(r)}, want after='B'`);
      }
      r = await client.toggleFormButton(1, 1);
      if (!r.toggled || r.after !== "B") {
        throw new Error(`issue-6240: radio 1 again -> ${JSON.stringify(r)}, want after='B'`);
      }

      // checkbox toggles both ways
      r = await client.toggleFormButton(1, 2);
      if (!r.toggled || r.after !== "Yes") {
        throw new Error(`issue-6240: checkbox on -> ${JSON.stringify(r)}, want after='Yes'`);
      }
      r = await client.toggleFormButton(1, 2);
      if (!r.toggled || r.after !== "Off") {
        throw new Error(`issue-6240: checkbox off -> ${JSON.stringify(r)}, want after='Off'`);
      }
      // answering the unsaved-changes prompt up front: the copy holds the
      // radio state and quitting below never shows the dialog
      await client.resolveUnsavedChanges("save-as", saved);
    },
    [pdf],
  );

  // the copy (compressed, so not greppable) must open with B still selected
  await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      await client.waitForRenderIdle();
      const r = await client.toggleFormButton(1, 1);
      if (r.before !== "B" || r.after !== "B") {
        throw new Error(`issue-6240: saved copy -> ${JSON.stringify(r)}, want before='B'`);
      }
      console.log("issue-6240: radio group and checkbox toggle, save-as ✓");
    },
    [saved],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
