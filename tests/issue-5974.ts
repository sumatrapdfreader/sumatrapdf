// #5974: the Annotations window opened taller than the screen. Matching the
// main window's canvas height ignored window chrome, and selecting an
// annotation grew the HWND to fit every field via LayoutAndSizeToContent.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath, waitForAnnotWindow } from "./util";
import { getWindowRect, getWorkArea, postMessage, sleep, WM_CLOSE } from "./winapi";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

function makePdf(): string {
  const annot = `<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (tester) /Contents (hello) >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot}] >>`,
  ];
  return assemblePdf(objs);
}

function assertFitsWorkArea(hwnd: number, what: string): void {
  const wa = getWorkArea();
  const r = getWindowRect(hwnd);
  const winDx = r.right - r.left;
  const winDy = r.bottom - r.top;
  const workDx = wa.right - wa.left;
  const workDy = wa.bottom - wa.top;
  if (winDx > workDx || winDy > workDy) {
    throw new Error(
      `issue-5974: ${what} ${winDx}x${winDy} is larger than the work area ${workDx}x${workDy} ` +
        `(win ${r.left},${r.top} work ${wa.left},${wa.top}-${wa.right},${wa.bottom})`,
    );
  }
}

async function selectFirstAnnot(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 1]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "");
    if (exitCode === 0) {
      return;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5974: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5974: editor never ready: ${raw}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5974.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const wa = getWorkArea();
  const dx = wa.right - wa.left;
  const dy = wa.bottom - wa.top;
  const { proc, client, frame } = await launchControlled(["-window-pos", `${dx}x${dy}@${wa.left}x${wa.top}`, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdEditAnnotations"));
    const annotWin = await waitForAnnotWindow(proc.pid!, frame);
    assertFitsWorkArea(annotWin, "initial Annotations window");

    await selectFirstAnnot(client);
    assertFitsWorkArea(annotWin, "Annotations window after selecting an annotation");

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5974: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-5974: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
