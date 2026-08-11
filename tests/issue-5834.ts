// Test for issue #5834: "Allow Contents text input box in Annotation window to
// expand".
//
// The Contents box of the annotation editor was a fixed five lines, so a
// multi-line annotation stayed cut off however tall the window was. It now
// shares the spare vertical space with the list of annotations (see #3769), so
// making the window taller shows more of the annotation's text.

import { writeFileSync } from "node:fs";
import { cmdId, EXE, tmpPath } from "./util";
import {
  enumChildWindows,
  enumWindows,
  findChildWindow,
  getClassName,
  getWindowPid,
  getWindowRect,
  moveWindow,
  packCoords,
  postMessage,
  sendMessage,
  sleep,
  waitForTopWindow,
  MK_LBUTTON,
  WM_COMMAND,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
} from "./winapi";

// one page with a text annotation whose Contents needs many lines
function makePdf(): string {
  const contents = Array.from({ length: 12 }, (_, i) => `annotation line ${i + 1}`).join("\\n");
  const annot = `<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (tester) /Contents (${contents}) >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot}] >>`,
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
    if (getWindowRect(hwnd).bottom - getWindowRect(hwnd).top > 200) {
      res = hwnd;
      return false;
    }
    return true;
  });
  return res;
}

// the Contents box: the tallest Edit in the editor window
function findContentsEdit(editor: number): number {
  let best = 0;
  let bestDy = 0;
  enumChildWindows(editor, (hwnd) => {
    if (getClassName(hwnd) !== "Edit") {
      return true;
    }
    const r = getWindowRect(hwnd);
    const dy = r.bottom - r.top;
    if (dy > bestDy) {
      best = hwnd;
      bestDy = dy;
    }
    return true;
  });
  return best;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5834.pdf");
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
    // select the annotation, which is what shows the Contents box
    postMessage(listBox, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(30, 8));
    postMessage(listBox, WM_LBUTTONUP, 0, packCoords(30, 8));
    await sleep(1000);

    const measure = async (dy: number) => {
      moveWindow(ew, 200, 30, 520, dy);
      await sleep(800);
      const edit = findContentsEdit(ew);
      if (!edit) {
        throw new Error("could not find the Contents box");
      }
      const r = getWindowRect(edit);
      return r.bottom - r.top;
    };

    const short = await measure(560);
    const tall = await measure(1000);
    if (tall <= short + 20) {
      throw new Error(
        `the Contents box does not use the extra space: ${short}px in a 560px tall window, ` +
          `${tall}px in a 1000px tall one`,
      );
    }
    console.log(`  Contents box grows with the window: ${short}px -> ${tall}px ✓`);
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
