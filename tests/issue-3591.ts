// Test for issue #3591: "Zoom position resets in book view when switching PDFs".
//
// In book / facing view the horizontal scroll jumped to the far right whenever a
// view was saved and restored (tab switch, session restore, window resize) while
// the first visible page was the *second* page of its row. All of those go
// through DisplayModel::Get/SetScrollState(), and SetScrollState() passes an
// absolute canvas x to GoToPage(); GoToPage() then added the width of the first
// page of the row on top of it ("scrollToNextPage"), scrolling a whole page too
// far right (clamped to the end of the canvas).
//
// The test scrolls right in book view until only the second page of a row is
// visible, then resizes the window *vertically* -- that runs the
// Get/SetScrollState round trip in SetViewPortSize() while leaving the
// horizontal layout untouched, so the horizontal scroll position must not move.
// Without the fix it ends up at the maximum instead.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, tmpPath } from "./util";
import { findCanvas, launchControlled, sendCommandSync, killAndWait } from "./win-automation";
import {
  getScrollInfo,
  getWindowRect,
  isZoomed,
  moveWindow,
  sendMessage,
  showWindow,
  SB_HORZ,
  SW_RESTORE,
} from "./winapi";

const WM_HSCROLL = 0x0114;
const SB_PAGERIGHT = 3;

// minimal, valid N-page PDF (Letter-size blank pages), ASCII only so string
// length == byte length (keeps the xref offsets correct).
function makePdf(nPages: number): string {
  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R >>`);
  const kids = Array.from({ length: nPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${nPages} /Kids [${kids}] >>`);
  for (let i = 0; i < nPages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`);
  }
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

const SETTINGS = [
  `DefaultDisplayMode = book view`,
  `DefaultZoom = 400`,
  `Scrollbars = windows`,
  `RestoreSession = false`,
  `ShowStartPage = false`,
  `CheckForUpdates = false`,
  ``,
].join("\n");

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-3591.pdf");
  writeFileSync(pdf, makePdf(6), "latin1");

  const appdata = tmpPath("issue-3591-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  // kill stale dev-build instances so reuse-instance can't forward our launch
  // to an old window (which would leave our process window-less)
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf], { defaultWindowPos: true });
  try {
    await client.waitForRenderIdle();
    if (isZoomed(frame)) {
      showWindow(frame, SW_RESTORE);
    }
    moveWindow(frame, 60, 60, 1200, 900);
    await client.waitForRenderIdle();

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("could not find the canvas window");
    }

    // page 3 is the second page of the [2,3] row (page 1 is the cover, alone)
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));

    // scroll right until the row's first page is off-screen to the left
    let h = 0;
    for (let i = 0; i < 8; i++) {
      const si = getScrollInfo(canvas, SB_HORZ);
      if (si.pos > (si.max - si.page) * 0.6) {
        break;
      }
      sendMessage(canvas, WM_HSCROLL, BigInt(SB_PAGERIGHT), 0n);
      if (getScrollInfo(canvas, SB_HORZ).pos === si.pos) {
        break; // can't scroll any further
      }
    }
    const siBefore = getScrollInfo(canvas, SB_HORZ);
    h = siBefore.pos;
    const hMax = siBefore.max - siBefore.page;
    if (h <= 0 || hMax <= 0) {
      throw new Error(`no horizontal scrolling happened (pos=${h}, max=${hMax})`);
    }
    if (h >= hMax) {
      throw new Error(`test setup: scrolled all the way right (pos=${h}, max=${hMax}), can't detect the jump`);
    }

    // shrink the window vertically: the horizontal layout doesn't change, so the
    // Get/SetScrollState round trip in SetViewPortSize() must preserve x
    const r = getWindowRect(frame);
    moveWindow(frame, r.left, r.top, r.right - r.left, r.bottom - r.top - 60);
    await client.waitForRenderIdle();

    const after = getScrollInfo(canvas, SB_HORZ).pos;
    if (Math.abs(after - h) > 2) {
      throw new Error(
        `horizontal scroll moved on resize: was ${h}, now ${after} (max ${hMax})` +
          (after >= hMax ? " -- jumped to the far right, issue #3591" : ""),
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
