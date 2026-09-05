// Closing the window with the Annotations list open used to crash: tab
// destructors refreshed the filter via CurrentTab(), which still pointed at a
// tab TabsCtrl held after that WindowTab had been deleted.
//
// Run: bun tests/annot-filter-close-window.ts [--no-build]
// The UAF is silent in a normal debug build; ASan reports it:
//   bun tests/annot-filter-close-window.ts --no-build -exe out/dbg64_asan/SumatraPDF-static.exe

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { findTopWindow, isWindowVisible, postMessage, sleep, WM_CLOSE } from "./winapi";
import { killAndWait, launchControlled, sendCommandSync, waitForExit } from "./win-automation";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";

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
  const dir = tmpPath("annot-filter-close-window");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfA = join(dir, "a.pdf");
  const pdfB = join(dir, "b.pdf");
  writeFileSync(pdfA, makePdf(), "latin1");
  writeFileSync(pdfB, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdfA, pdfB]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // selected tab must not be last: DeleteVecMembers walks 0..n-1, and
    // CurrentTab() kept returning the already-deleted selected tab
    sendCommandSync(frame, cmdId("CmdPrevTab"));
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300 * SLOW_BUILD_FACTOR);
    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    let floatWnd = 0;
    while (Date.now() < deadline) {
      floatWnd = findTopWindow(proc.pid!, FLOAT_CLASS);
      if (floatWnd && isWindowVisible(floatWnd)) {
        break;
      }
      await sleep(50);
    }
    if (!floatWnd) {
      throw new Error("annot-filter-close-window: the Annotations window did not open");
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    const exited = await waitForExit(proc, 8000 * SLOW_BUILD_FACTOR);
    if (!exited) {
      throw new Error("annot-filter-close-window: window did not close");
    }
    const code = proc.exitCode;
    if (code !== 0 && code !== null) {
      throw new Error(`annot-filter-close-window: app crashed on close (exit ${code})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
