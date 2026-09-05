// The Annotations window caches non-owning Annotation* from the tab's engine.
// Closing the tab freed them while the list was still on screen, so the next
// paint read freed memory (ASan heap-use-after-free in DrawAnnotationListRow).
//
// Run: bun tests/annot-filter-close-tab.ts [--no-build]
// Run against the ASan build to have the read actually reported:
//   SUMATRA_TEST_EXE=out/dbg64_asan/SumatraPDF-static.exe bun tests/annot-filter-close-tab.ts --no-build

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { sleep } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

// two Text annotations, enough for the list to have rows to paint
function makePdf(): string {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
  objs[3] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 300] /Annots [4 0 R 5 0 R] >>";
  objs[4] = "<< /Type /Annot /Subtype /Text /Rect [20 240 40 260] /T (kjk) /Contents (first note) >>";
  objs[5] = "<< /Type /Annot /Subtype /Text /Rect [20 200 40 220] /T (bob) /Contents (second note) >>";

  let pdf = "%PDF-1.7\n";
  const offsets: number[] = [];
  for (let i = 1; i < objs.length; i++) {
    offsets[i] = pdf.length;
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = pdf.length;
  pdf += `xref\n0 ${objs.length}\n0000000000 65535 f \n`;
  for (let i = 1; i < objs.length; i++) {
    pdf += `${String(offsets[i]).padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-filter-close-tab");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindAnnotation"]);
    const opened = await client.request(ControlCommand.TestAnnotFilter, []);
    if (!/floatVisible=1/.test(String(opened[1] ?? ""))) {
      throw new Error(`annot-filter-close-tab: the Annotations window did not open: ${String(opened[1] ?? "")}`);
    }
    await sleep(300 * SLOW_BUILD_FACTOR);

    await client.request(ControlCommand.TestInvokeCommand, ["CmdClose"]);
    await sleep(400 * SLOW_BUILD_FACTOR);

    for (let i = 0; i < 3; i++) {
      await client.request(ControlCommand.TestAnnotFilter, ["paint"]);
      await sleep(150 * SLOW_BUILD_FACTOR);
    }
    if (proc.exitCode !== null) {
      throw new Error(`annot-filter-close-tab: the app died after closing the tab (exit ${proc.exitCode})`);
    }
    // a round trip proves the UI thread is still alive and serving; it must
    // not need a document, there is none left
    await client.setNotificationsEnabled(false);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
