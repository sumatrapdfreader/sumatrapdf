// Test for issue #3769: "annotation editor's list of annotations is not
// consistently sized".
//
// The list of annotations used to be a fixed number of lines - 5, or 14 once
// the window was taller than 1024 - so it stayed the same size no matter how
// big the window was (a 1000px tall editor left ~750px of empty space below the
// list), and a small difference in window height flipped it between two very
// different sizes. It now takes whatever vertical space the rest of the window
// doesn't need.
//
// The test opens the annotation editor, resizes it and checks the list follows:
// the space below the list stays about the same while the list itself grows and
// shrinks with the window.

import { writeFileSync } from "node:fs";
import { cmdId, EXE, tmpPath } from "./util";
import {
  enumWindows,
  findChildWindow,
  getClassName,
  getWindowPid,
  getWindowRect,
  moveWindow,
  sendMessage,
  sleep,
  waitForTopWindow,
  WM_COMMAND,
} from "./winapi";

// a one-page PDF; annotations can be added to it, which is all the editor needs
function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

// the editor is the process' top-level window that isn't the main frame
function findEditorWindow(pid: number, frame: number): number {
  let res = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || hwnd === frame) {
      return true;
    }
    const cls = getClassName(hwnd);
    if (cls.includes("SUMATRA") || cls.length === 0) {
      return true;
    }
    const r = getWindowRect(hwnd);
    if (r.bottom - r.top > 200) {
      res = hwnd;
      return false;
    }
    return true;
  });
  return res;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3769.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);
  const proc = Bun.spawn([EXE, "-for-testing", pdf], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForTopWindow(proc.pid, "SUMATRA_PDF_FRAME");
    if (!frame) {
      throw new Error("SumatraPDF main window did not appear");
    }
    await sleep(1500);
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdEditAnnotations")), 0n);
    await sleep(1800);

    const ew = findEditorWindow(proc.pid, frame);
    if (!ew) {
      throw new Error("the annotation editor window did not appear");
    }
    const listBox = findChildWindow(ew, "ListBox");
    if (!listBox) {
      throw new Error("could not find the list of annotations");
    }

    const measure = async (dy: number) => {
      moveWindow(ew, 200, 30, 520, dy);
      await sleep(700);
      const wr = getWindowRect(ew);
      const lr = getWindowRect(listBox);
      return { windowDy: wr.bottom - wr.top, listDy: lr.bottom - lr.top, gapBelow: wr.bottom - lr.bottom };
    };

    const tall = await measure(1000);
    const short = await measure(560);
    const tallAgain = await measure(1000);

    // the list has to follow the window, not stay at a fixed number of lines
    const grew = tall.listDy - short.listDy;
    const windowGrew = tall.windowDy - short.windowDy;
    if (grew < windowGrew / 2) {
      throw new Error(
        `the list does not take the available space: window ${short.windowDy} -> ${tall.windowDy} ` +
          `but list only ${short.listDy} -> ${tall.listDy}`,
      );
    }
    // no big empty area under the list
    if (tall.gapBelow > 200) {
      throw new Error(`${tall.gapBelow}px of unused space below the list in a ${tall.windowDy}px tall window`);
    }
    // and the same window size gives the same layout every time
    if (Math.abs(tallAgain.listDy - tall.listDy) > 4) {
      throw new Error(`same window height gave different list sizes: ${tall.listDy} then ${tallAgain.listDy}`);
    }
    console.log(
      `  annotation list follows the window: ${short.listDy}px at ${short.windowDy} -> ` +
        `${tall.listDy}px at ${tall.windowDy} (gap below ${tall.gapBelow}px) ✓`,
    );
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
