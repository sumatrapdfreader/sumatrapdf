// Issue #5934: close the translate dialog with Esc / Ctrl+W.
//
// Esc is exercised with posted WM_KEYDOWN (no modifiers). Ctrl+W needs
// GetKeyState(VK_CONTROL) which posted messages do not set, so that path is
// covered by code review / manual check rather than this script.

import { writeFileSync } from "node:fs";
import { cmdId, tmpPath } from "./util";
import { launchControlled, killAndWait } from "./win-automation";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sendMessage,
  sleep,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi";

function makeTextPdf(): Buffer {
  const line = "hello translation window";
  const content = `BT /F1 24 Tf 72 720 Td (${line}) Tj ET`;
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
  ];
  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefPos = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefPos}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

// top-level window of this process that is not the main frame and looks like a
// secondary dialog (captioned, reasonably tall)
function findSecondaryWindow(pid: number, frame: number, titleIncludes?: string): number {
  let res = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || hwnd === frame) {
      return true;
    }
    if (!isWindowVisible(hwnd)) {
      return true;
    }
    const cls = getClassName(hwnd);
    if (cls.includes("SUMATRA") || cls.length === 0) {
      return true;
    }
    if (titleIncludes) {
      const title = getWindowText(hwnd);
      if (!title.toLowerCase().includes(titleIncludes.toLowerCase())) {
        return true;
      }
    }
    res = hwnd;
    return false;
  });
  return res;
}

function pressEscape(hwnd: number): void {
  postMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
  postMessage(hwnd, WM_KEYUP, VK_ESCAPE, 0);
}

async function waitForSecondary(
  pid: number,
  frame: number,
  titleIncludes: string | undefined,
  timeoutMs: number,
): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findSecondaryWindow(pid, frame, titleIncludes);
    if (h) {
      return h;
    }
    await sleep(100);
  }
  return 0;
}

async function testTranslateEscCloses(): Promise<void> {
  const pdf = tmpPath("issue-5934-translate.pdf");
  writeFileSync(pdf, makeTextPdf());

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();

    // select some text so CmdTranslateSelection has something to show
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdSelectTextViaKeyboard")), 0n);
    for (let i = 0; i < 5; i++) {
      sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdExtendSelectionWordRight")), 0n);
    }

    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdTranslateSelection")), 0n);
    const tw = await waitForSecondary(proc.pid!, frame, "translate", 4000);
    if (!tw) {
      throw new Error("translate: dialog did not appear (selection empty?)");
    }

    pressEscape(tw);
    const deadline = Date.now() + 3000;
    while (Date.now() < deadline) {
      if (!isWindowVisible(tw) && !findSecondaryWindow(proc.pid!, frame, "translate")) {
        console.log("  translate: Esc closes ✓");
        return;
      }
      await sleep(30);
    }
    throw new Error("translate: Esc did not close the translate dialog");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await testTranslateEscCloses();
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
