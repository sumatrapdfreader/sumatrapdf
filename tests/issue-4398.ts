// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4398
//
// CmdTogglePageGrid overlays graph paper on paginated documents (default
// ¼ inch minor / 1 inch major, color 128,128,255, dots). Showing it is
// session-only. CmdConfigurePageGrid sets spacing, origin, color and style
// (saved in FixedPageUI.PageGrid).
//
// The fixture is tests/issue-4398.pdf (opaque white page + text). Regenerated
// by this file when run as main.
//
// Run: bun tests/issue-4398.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, runStandalone } from "./util.ts";
import {
  captureWindowPixels,
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowText,
  sendMessage,
  sleep,
} from "./winapi.ts";
import { findCanvas, pressEscape, sendCommand, waitForFrame } from "./win-automation.ts";

const BM_CLICK = 0x00f5;

const PDF = join(ROOT, "tests", "issue-4398.pdf");

// Opaque white page so the overlay is on paper, not in a hole.
export function makePageGridPdf(): string {
  const stream = ["1 1 1 rg", "0 0 400 300 re f", "0 0 0 rg", "BT /F1 18 Tf 40 220 Td (Page grid) Tj ET"].join("\n");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 300] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  body += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

async function waitForPageGridDialog(pid: number, timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let found = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) === pid && getWindowText(hwnd) === "Page Grid") {
        found = hwnd;
      }
      return true;
    });
    if (found) {
      return found;
    }
    await sleep(30);
  }
  throw new Error("issue-4398: Page Grid dialog did not appear");
}

function findShowGridCheckbox(dlg: number): number {
  let found = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === "Button" && getControlText(hwnd).replace(/&/g, "") === "Show Grid") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function countChildClass(dlg: number, className: string): number {
  let n = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === className) {
      n++;
    }
    return true;
  });
  return n;
}

function countGridColor(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    // kPageGridColor is RGB(128, 128, 255)
    if (Math.abs(r - 128) <= 8 && Math.abs(g - 128) <= 8 && Math.abs(b - 255) <= 8) {
      n++;
    }
  }
  return n;
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    writeFileSync(PDF, makePageGridPdf());
  }

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-4398: no canvas");
      }
      const before = captureWindowPixels(canvas);
      if (!before) {
        throw new Error("issue-4398: capture failed");
      }
      const nBefore = countGridColor(before.data);

      sendCommand(frame, cmdId("CmdTogglePageGrid"));
      await client.waitForRenderIdle(30000);
      const after = captureWindowPixels(canvas);
      if (!after) {
        throw new Error("issue-4398: capture after toggle failed");
      }
      const nAfter = countGridColor(after.data);
      if (nAfter < nBefore + 40) {
        throw new Error(`issue-4398: page grid did not show (grid pixels before=${nBefore} after=${nAfter})`);
      }
      console.log(`  grid pixels ${nBefore} -> ${nAfter} ✓`);

      sendCommand(frame, cmdId("CmdTogglePageGrid"));
      await client.waitForRenderIdle(30000);

      sendCommand(frame, cmdId("CmdConfigurePageGrid"));
      const dlg = await waitForPageGridDialog(proc.pid!);
      const readyDeadline = Date.now() + 3000;
      let cb = 0;
      while (Date.now() < readyDeadline) {
        cb = findShowGridCheckbox(dlg);
        if (cb && countChildClass(dlg, "ComboBox") >= 2 && countChildClass(dlg, "Edit") >= 5) {
          break;
        }
        await sleep(30);
      }
      if (!cb) {
        throw new Error("issue-4398: Show Grid checkbox not found");
      }
      sendMessage(cb, BM_CLICK, 0, 0);
      await client.waitForRenderIdle(30000);
      const fromDlg = captureWindowPixels(canvas);
      if (!fromDlg) {
        throw new Error("issue-4398: capture after configure dialog failed");
      }
      const nDlg = countGridColor(fromDlg.data);
      if (nDlg < nBefore + 40) {
        throw new Error(`issue-4398: Show Grid in configure dialog did not show overlay (grid pixels=${nDlg})`);
      }
      console.log(`  configure dialog Show Grid pixels ${nDlg} ✓`);
      await pressEscape(dlg);
    },
    [PDF],
  );
}

if (import.meta.main) {
  writeFileSync(PDF, makePageGridPdf());
  const bugs = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-4398.pdf";
  try {
    writeFileSync(bugs, makePageGridPdf());
    console.log(`wrote ${PDF} and ${bugs}`);
  } catch {
    console.log(`wrote ${PDF}`);
  }
  await runStandalone(testit);
}
