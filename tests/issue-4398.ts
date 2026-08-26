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

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, runStandalone, tmpPath, assemblePdf } from "./util.ts";
import {
  captureWindowPixels,
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getControlText,
  getWindowPid,
  getWindowText,
  sendMessage,
  sendText,
  sleep,
} from "./winapi.ts";
import { clickAt, findCanvas, pressEscape, sendCommand, waitForFrame } from "./win-automation.ts";

const BM_GETCHECK = 0x00f0;
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
  return assemblePdf(objs);
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

function childWindowsByClass(dlg: number, className: string): number[] {
  const windows: number[] = [];
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === className) {
      windows.push(hwnd);
    }
    return true;
  });
  return windows;
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

  const appdata = tmpPath("issue-4398-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

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

      const edits = childWindowsByClass(dlg, "Edit");
      if (edits.length !== 6) {
        throw new Error(`issue-4398: expected 6 page-grid edit controls, got ${edits.length}`);
      }
      sendText(edits[0]!, "2");
      await sleep(150);
      if (getControlText(edits[0]!) !== "2") {
        throw new Error("issue-4398: failed to change a page-grid value before reset");
      }

      const dlgRect = getClientRect(dlg);
      await clickAt(dlg, Math.round(dlgRect.right * 0.18), Math.round(dlgRect.bottom * 0.93));
      const values = edits.map((hwnd) => getControlText(hwnd)).sort();
      const defaults = ["#8080ff", "0", "0", "1", "1", "4"].sort();
      if (values.join("|") !== defaults.join("|")) {
        throw new Error(`issue-4398: Reset to defaults produced ${values.join(", ")}`);
      }
      if (Number(sendMessage(cb, BM_GETCHECK, 0, 0)) !== 1) {
        throw new Error("issue-4398: Reset to defaults turned off Show Grid");
      }
      console.log("  Reset to defaults restored all page-grid fields and kept Show Grid on ✓");
      await pressEscape(dlg);
    },
    ["-appdata", appdata, PDF],
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
