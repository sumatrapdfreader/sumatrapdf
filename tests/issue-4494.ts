// #4494: remember the last-used Annotations window size across documents and
// application sessions.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath, waitForAnnotWindow } from "./util";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowRect,
  getWindowText,
  getWorkArea,
  postMessage,
  setWindowPos,
  WM_CLOSE,
} from "./winapi";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

function makePdf(contents: string): string {
  const annot = `<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (tester) /Contents (${contents}) >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot}] >>`,
  ];
  return assemblePdf(objs);
}

async function openAnnotWindow(client: ControlClient, pid: number, frame: number): Promise<number> {
  sendCommand(frame, cmdId("CmdEditAnnotations"));
  // Synchronize with the UI thread so we don't observe CreateCustom's initial
  // full-screen-sized HWND before ShowEditAnnotationsWindow finishes sizing it.
  const layout = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  if ((layout[0] as number) !== 0) {
    throw new Error(`issue-4494: editor didn't finish opening: ${String(layout[1] ?? "")}`);
  }
  return waitForAnnotWindow(pid, frame);
}

function windowSize(hwnd: number): { dx: number; dy: number } {
  const r = getWindowRect(hwnd);
  return { dx: r.right - r.left, dy: r.bottom - r.top };
}

function processWindows(pid: number): string {
  const windows: string[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid) {
      const r = getWindowRect(hwnd);
      windows.push(`${hwnd} ${getClassName(hwnd)} ${r.right - r.left}x${r.bottom - r.top} "${getWindowText(hwnd)}"`);
    }
    return true;
  });
  return windows.join("; ");
}

async function closeApp(proc: Bun.Subprocess, frame: number): Promise<void> {
  postMessage(frame, WM_CLOSE, 0, 0);
  if (!(await waitForExit(proc))) {
    throw new Error("issue-4494: SumatraPDF didn't exit after WM_CLOSE");
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-4494-data");
  const appdata = join(dir, "appdata");
  const firstPdf = join(dir, "first.pdf");
  const secondPdf = join(dir, "second.pdf");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "RestoreSession = false\nUseTabs = true\n");
  writeFileSync(firstPdf, makePdf("first"), "latin1");
  writeFileSync(secondPdf, makePdf("second"), "latin1");

  let remembered = { dx: 0, dy: 0 };
  const first = await launchControlled(["-appdata", appdata, firstPdf], { saveSettings: true });
  try {
    await first.client.waitForRenderIdle();
    const annot = await openAnnotWindow(first.client, first.proc.pid!, first.frame);
    const initial = windowSize(annot);
    const work = getWorkArea();
    const maxDx = work.right - work.left - 40;
    const maxDy = work.bottom - work.top - 40;
    const targetDx = Math.min(initial.dx >= 650 ? 570 : 690, maxDx);
    const targetDy = Math.min(initial.dy >= 480 ? 410 : 530, maxDy);
    const r = getWindowRect(annot);
    setWindowPos(annot, r.left, r.top, targetDx, targetDy);
    remembered = windowSize(annot);
    if (Math.abs(remembered.dx - initial.dx) < 20 || Math.abs(remembered.dy - initial.dy) < 20) {
      throw new Error(
        `issue-4494: test resize was too small: ${initial.dx}x${initial.dy} -> ${remembered.dx}x${remembered.dy}`,
      );
    }
    await closeApp(first.proc, first.frame);
  } finally {
    first.client.close();
    await killAndWait(first.proc);
  }

  const second = await launchControlled(["-appdata", appdata, secondPdf], { saveSettings: true });
  try {
    await second.client.waitForRenderIdle();
    const annot = await openAnnotWindow(second.client, second.proc.pid!, second.frame);
    const restored = windowSize(annot);
    if (Math.abs(restored.dx - remembered.dx) > 2 || Math.abs(restored.dy - remembered.dy) > 2) {
      throw new Error(
        `issue-4494: Annotations size wasn't restored: remembered ${remembered.dx}x${remembered.dy}, ` +
          `got ${restored.dx}x${restored.dy}; windows: ${processWindows(second.proc.pid!)}`,
      );
    }
    console.log(`  Annotations window size restored across PDFs: ${restored.dx}x${restored.dy} ✓`);
    await closeApp(second.proc, second.frame);
  } finally {
    second.client.close();
    await killAndWait(second.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
