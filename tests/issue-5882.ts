// Regression test for issue #5882: rows painted over each other after scrolling
// the Advanced Settings list.
//
// A listbox scrolls by moving its existing pixels and invalidating only the
// strip that was newly uncovered. Owner-draw item rects used to be shrunk to
// fit inside the client area, so the bottom row -- always cut off, the list
// uses LBS_NOINTEGRALHEIGHT -- was drawn into a short rect: its text landed off
// center and the rest of the row was never painted. Scrolling then carried
// those unpainted pixels up into the middle of the list, showing whatever had
// scrolled underneath: two sets of rows on top of each other.
//
// Checked structurally, on the real pixels: a correctly drawn list of
// single-line rows has a clear horizontal gap between one row's text and the
// next, so a good fraction of the pixel rows contain no text at all. When rows
// are drawn over each other the ghosts fill those gaps in.

import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getWindowPid,
  getWindowRect,
  moveWindow,
  postMessage,
  readWindowDCColumn,
  sendMessage,
  sleep,
} from "./winapi.ts";
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation.ts";
import { cmdId, runStandalone } from "./util.ts";

const WM_VSCROLL = 0x0115;
const SB_LINEDOWN = 1;
const WM_CLOSE = 0x0010;
const LB_GETITEMHEIGHT = 0x01a1;

// how much of the blank space between rows scrolling may eat. Ghost rows fill
// the gaps in, which cost ~22 points when this bug was live
const MAX_BLANK_ROW_LOSS = 0.1;

function findDialogWithListBox(pid: number, frame: number): { dlg: number; list: number } {
  let dlg = 0;
  enumWindows((h) => {
    if (getWindowPid(h) !== pid || h === frame) {
      return true;
    }
    let hasList = false;
    enumChildWindows(h, (c) => {
      if (getClassName(c) === "ListBox") {
        hasList = true;
        return false;
      }
      return true;
    });
    if (hasList) {
      dlg = h;
      return false;
    }
    return true;
  });
  let list = 0;
  if (dlg) {
    enumChildWindows(dlg, (c) => {
      if (getClassName(c) === "ListBox") {
        list = c;
        return false;
      }
      return true;
    });
  }
  return { dlg, list };
}

// per pixel row of the setting-name column: does it contain any text?
function sampleBlankRows(list: number, listDx: number, listDy: number): boolean[] {
  const x0 = Math.floor(listDx * 0.02);
  const x1 = Math.floor(listDx * 0.45);
  const nCols = 24;
  const cols: number[][] = [];
  for (let i = 0; i < nCols; i++) {
    cols.push(readWindowDCColumn(list, x0 + Math.floor(((x1 - x0) * i) / nCols), 0, listDy));
  }
  // the background is whatever color dominates the sample
  const counts = new Map<number, number>();
  for (const col of cols) {
    for (const c of col) {
      counts.set(c, (counts.get(c) ?? 0) + 1);
    }
  }
  let bg = 0;
  let bgCount = -1;
  for (const [c, n] of counts) {
    if (n > bgCount) {
      bg = c;
      bgCount = n;
    }
  }
  const isBlank: boolean[] = [];
  for (let y = 0; y < listDy; y++) {
    let ink = 0;
    for (const col of cols) {
      if (col[y] !== bg) {
        ink++;
      }
    }
    isBlank.push(ink === 0);
  }
  return isBlank;
}

function blankRatio(isBlank: boolean[]): number {
  return isBlank.filter(Boolean).length / isBlank.length;
}

export async function testit(): Promise<void> {
  const proc = launchSumatra([]);
  try {
    const frame = await waitForFrame(proc.pid!);
    await sleep(1200);
    sendCommand(frame, cmdId("CmdAdvancedSettings"));
    await sleep(1500);

    const { dlg, list } = findDialogWithListBox(proc.pid!, frame);
    if (!dlg || !list) {
      throw new Error("issue-5882: Advanced Settings dialog / list not found");
    }

    const itemH = Number(sendMessage(list, LB_GETITEMHEIGHT, 0, 0));
    if (itemH <= 1) {
      throw new Error(`issue-5882: bogus item height ${itemH}`);
    }
    // Show about half of the bottom row: that is where the row was drawn into
    // the most-shrunken rect, so the mis-centering and the strip it never
    // painted are largest.
    const wr = getWindowRect(dlg);
    const want = Math.floor(itemH / 2);
    let best = { extra: 0, leftover: -1 };
    for (let extra = 0; extra < itemH; extra++) {
      moveWindow(dlg, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top + extra, true);
      await sleep(120);
      const r = getClientRect(list);
      const leftover = (r.bottom - r.top) % itemH;
      if (best.leftover < 0 || Math.abs(leftover - want) < Math.abs(best.leftover - want)) {
        best = { extra, leftover };
      }
      if (leftover === want) {
        break;
      }
    }
    if (Math.abs(best.leftover - want) > 2) {
      throw new Error(`issue-5882: could not get a half-visible bottom row (best leftover ${best.leftover}/${itemH})`);
    }
    moveWindow(dlg, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top + best.extra, true);
    await sleep(600);
    const cr = getClientRect(list);
    const listDy = cr.bottom - cr.top;
    const listDx = cr.right - cr.left;

    // Baseline from the freshly laid out list: one line of text per row, so a
    // fixed share of pixel rows is the blank gap between lines. Every row on
    // screen has exactly one line whatever the setting names are, so this is a
    // property of the geometry -- it must not change when the list scrolls.
    const before = blankRatio(sampleBlankRows(list, listDx, listDy));
    if (before < 0.2) {
      throw new Error(`issue-5882: list already looks wrong before scrolling (blank rows ${before.toFixed(2)})`);
    }

    // scroll a row at a time, the way the report does
    for (let i = 0; i < 40; i++) {
      sendMessage(list, WM_VSCROLL, SB_LINEDOWN, 0);
      await sleep(120);
    }
    await sleep(700);

    const after = blankRatio(sampleBlankRows(list, listDx, listDy));
    if (after < before - MAX_BLANK_ROW_LOSS) {
      throw new Error(
        `issue-5882: scrolling filled in the gaps between rows: ${(before * 100).toFixed(0)}% of pixel ` +
          `rows were blank before, ${(after * 100).toFixed(0)}% after - rows are drawn over each other`,
      );
    }
    console.log(
      `issue-5882: OK itemHeight=${itemH} leftover=${best.leftover} ` +
        `blank rows ${(before * 100).toFixed(0)}% -> ${(after * 100).toFixed(0)}%`,
    );

    postMessage(dlg, WM_CLOSE, 0, 0);
    await sleep(400);
    sendCommand(frame, cmdId("CmdExit"));
    await sleep(500);
  } finally {
    try {
      proc.kill();
    } catch {}
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
