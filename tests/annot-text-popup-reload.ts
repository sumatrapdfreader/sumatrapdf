// The annotation comment card holds an Annotation* into the engine. A reload
// replaces the engine; the next repaint repositioned the card through the
// freed annotation and crashed in AnnotationIsLive.
//
// Run: bun tests/annot-text-popup-reload.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, SLOW_BUILD_FACTOR, cmdId, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, clickAt, findCanvas, sendCommandSync } from "./win-automation.ts";
import { getWindowRect, isWindowVisible, sleep, waitForTopWindow } from "./winapi.ts";

const POPUP_CLASS = "SumatraAnnotTextPopup";

// one page covered by a Square annotation with a comment
function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> /Annots [4 0 R] >>`,
    `<< /Type /Annot /Subtype /Square /Rect [20 20 592 772] /C [1 0 0] /Contents (a comment) >>`,
  ];
  let pdf = "%PDF-1.7\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = pdf.length;
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = pdf.length;
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (const o of off) {
    pdf += o.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-text-popup-reload");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      sendCommandSync(frame, cmdId("CmdZoomFitPage"));
      await client.waitForRenderIdle();

      // a click on the annotation shows its comment card
      const canvas = findCanvas(frame);
      const r = getWindowRect(canvas);
      await clickAt(canvas, Math.floor((r.right - r.left) / 2), Math.floor((r.bottom - r.top) / 2));
      const popup = await waitForTopWindow(proc.pid!, POPUP_CLASS, 4000 * SLOW_BUILD_FACTOR);
      if (!popup || !isWindowVisible(popup)) {
        throw new Error("comment card did not show");
      }

      // reload, then repaint: the card must not outlive the old engine
      sendCommandSync(frame, cmdId("CmdReloadDocument"));
      await client.waitForRenderIdle();
      for (let i = 0; i < 5; i++) {
        sendCommandSync(frame, cmdId("CmdScrollDown"));
        await client.waitForRenderIdle();
        await sleep(20);
      }
      if (isWindowVisible(popup)) {
        throw new Error("comment card still shown after reload");
      }
    },
    [pdf],
  );
  rmSync(dir, { recursive: true, force: true });
  console.log("annot-text-popup-reload: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
