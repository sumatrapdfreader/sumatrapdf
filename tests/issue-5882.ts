// Regression test for issue #5882: rows painted over each other after scrolling
// the Advanced Settings list.
//
// A list scrolls by moving its existing pixels and invalidating only the strip
// that was newly uncovered. Item rects used to be shrunk to fit inside the
// client area, so the bottom row -- always cut off, because the client height
// isn't a whole number of rows -- was drawn into a short rect: its text landed
// off center and the rest of the row was never painted. Scrolling then carried
// those unpainted pixels up into the middle of the list, showing whatever had
// scrolled underneath: two sets of rows on top of each other.
//
// The list is a VirtListBox now, with no HWND of its own, so the geometry comes
// from the app (TestAdvSettingsRows) instead of from LB_GETITEMHEIGHT and the
// listbox's client rect. Two things are checked:
//
//   - structurally: every drawn row is a whole row, rows tile the viewport
//     without gaps or overlap, and none of them reaches into the leftover strip
//     below the last whole row. That is the shrunken-rect bug stated directly.
//   - on the real pixels: a correctly drawn list of single-line rows has a clear
//     horizontal gap between one row's text and the next, so a good fraction of
//     the pixel rows contain no text at all. Ghost rows fill those gaps in. The
//     leftover strip at the bottom must stay blank too.
//
// The dialog is sized so the leftover strip is about half a row: that is where
// the row used to be drawn into the most-shrunken rect.

import {
  clientToScreen,
  enumWindows,
  getWindowPid,
  getWindowRect,
  getWindowText,
  moveWindow,
  postMessage,
  readWindowDCColumn,
  sleep,
} from "./winapi.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { cmdId, EXE, runStandalone } from "./util.ts";

const WM_CLOSE = 0x0010;

// how much of the blank space between rows scrolling may eat. Ghost rows fill
// the gaps in, which cost ~22 points when this bug was live
const MAX_BLANK_ROW_LOSS = 0.1;

type Geom = {
  content: { x: number; y: number; dx: number; dy: number };
  itemDy: number;
  usableDy: number;
  scrollY: number;
  maxScrollY: number;
  items: number;
  rows: { idx: number; x: number; y: number; dx: number; dy: number }[];
};

function parseGeom(out: string): Geom {
  const m =
    /^OK content=(-?\d+),(-?\d+),(-?\d+),(-?\d+) itemDy=(\d+) usableDy=(\d+) scrollY=(\d+) maxScrollY=(\d+) items=(\d+)/m.exec(
      out,
    );
  if (!m) {
    throw new Error(`issue-5882: could not parse geometry: ${out.trim()}`);
  }
  const rows: Geom["rows"] = [];
  for (const line of out.split("\n")) {
    const r = /^row=(\d+) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)$/.exec(line.trim());
    if (r) {
      rows.push({ idx: +r[1]!, x: +r[2]!, y: +r[3]!, dx: +r[4]!, dy: +r[5]! });
    }
  }
  return {
    content: { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! },
    itemDy: +m[5]!,
    usableDy: +m[6]!,
    scrollY: +m[7]!,
    maxScrollY: +m[8]!,
    items: +m[9]!,
    rows,
  };
}

// "geom" reports what the list laid out; "scroll" first scrolls by `rows` rows
async function advSettingsRows(client: ControlClient, action: string, rows = 0): Promise<Geom> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action, rows]);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      return parseGeom(out);
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5882: TestAdvSettingsRows(${action}) failed: ${out.trim()}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5882: settings list never laid out: ${out.trim()}`);
    }
    await sleep(100);
  }
}

function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid) {
      return true;
    }
    if (getWindowText(hwnd).includes("Advanced Settings")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// rows must be whole, contiguous, and inside the band of whole rows
function checkRowsTile(g: Geom, when: string): void {
  if (g.rows.length < 4) {
    throw new Error(`issue-5882: only ${g.rows.length} rows visible ${when}`);
  }
  const bandBottom = g.content.y + g.usableDy;
  let prev = g.rows[0]!;
  if (prev.y !== g.content.y) {
    throw new Error(`issue-5882: first row ${when} starts at y=${prev.y}, list content starts at ${g.content.y}`);
  }
  for (const r of g.rows) {
    if (r.dy !== g.itemDy) {
      throw new Error(`issue-5882: row ${r.idx} ${when} is ${r.dy}px tall, a row is ${g.itemDy}px`);
    }
    if (r.y + r.dy > bandBottom) {
      throw new Error(
        `issue-5882: row ${r.idx} ${when} ends at y=${r.y + r.dy}, past the last whole row at ${bandBottom}`,
      );
    }
    if (r !== prev && r.y !== prev.y + g.itemDy) {
      throw new Error(`issue-5882: row ${r.idx} ${when} starts at y=${r.y}, previous row ended at ${prev.y + prev.dy}`);
    }
    prev = r;
  }
}

// per pixel row of the setting-name column: does it contain any text?
// x/y are in the dialog's window-DC coordinates.
function sampleBlankRows(dlg: number, x0: number, x1: number, y: number, dy: number): boolean[] {
  const nCols = 24;
  const cols: number[][] = [];
  for (let i = 0; i < nCols; i++) {
    cols.push(readWindowDCColumn(dlg, x0 + Math.floor(((x1 - x0) * i) / nCols), y, dy));
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
  for (let i = 0; i < dy; i++) {
    let ink = 0;
    for (const col of cols) {
      if (col[i] !== bg) {
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

async function waitForDialog(pid: number, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const dlg = findDialog(pid);
    if (dlg) {
      return dlg;
    }
    await sleep(30);
  }
  throw new Error("issue-5882: Advanced Settings dialog not found");
}

export async function testit(): Promise<void> {
  await withControlledSumatra(EXE, async (client, proc) => {
    const frame = await waitForFrame(proc.pid!);
    sendCommand(frame, cmdId("CmdAdvancedSettings"));
    const dlg = await waitForDialog(proc.pid!);

    let g = await advSettingsRows(client, "geom");
    if (g.itemDy <= 1) {
      throw new Error(`issue-5882: bogus item height ${g.itemDy}`);
    }
    if (g.maxScrollY <= 0) {
      throw new Error("issue-5882: settings list doesn't scroll, nothing to test");
    }

    // Leave about half a row below the last whole one: that is the strip the
    // shrunken bottom row used to be drawn into.
    const want = Math.floor(g.itemDy / 2);
    const wr = getWindowRect(dlg);
    let best = { extra: 0, leftover: -1 };
    for (let extra = 0; extra < g.itemDy; extra++) {
      moveWindow(dlg, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top + extra, true);
      g = await advSettingsRows(client, "geom");
      const leftover = g.content.dy - g.usableDy;
      if (best.leftover < 0 || Math.abs(leftover - want) < Math.abs(best.leftover - want)) {
        best = { extra, leftover };
      }
      if (leftover === want) {
        break;
      }
    }
    if (Math.abs(best.leftover - want) > 2) {
      throw new Error(`issue-5882: could not leave a half-row strip (best leftover ${best.leftover}/${g.itemDy})`);
    }
    moveWindow(dlg, wr.left, wr.top, wr.right - wr.left, wr.bottom - wr.top + best.extra, true);

    g = await advSettingsRows(client, "geom");
    checkRowsTile(g, "before scrolling");

    // the probe reports client coords; the pixels are read from the window DC
    const org = clientToScreen(dlg, 0, 0);
    const wr2 = getWindowRect(dlg);
    const offX = org.x - wr2.left;
    const offY = org.y - wr2.top;

    // the setting-name column, and everything from the first row down to the
    // bottom of the list including the leftover strip
    const row0 = g.rows[0]!;
    const x0 = offX + row0.x + Math.floor(row0.dx * 0.02);
    const x1 = offX + row0.x + Math.floor(row0.dx * 0.45);
    const y = offY + g.content.y;
    const dy = g.content.dy;

    const before = blankRatio(sampleBlankRows(dlg, x0, x1, y, dy));
    if (before < 0.2) {
      throw new Error(`issue-5882: list already looks wrong before scrolling (blank rows ${before.toFixed(2)})`);
    }

    // scroll a row at a time, the way the report does
    for (let i = 0; i < 40; i++) {
      await advSettingsRows(client, "scroll", 1);
    }

    g = await advSettingsRows(client, "geom");
    checkRowsTile(g, "after scrolling");
    if (g.scrollY === 0) {
      throw new Error("issue-5882: list never scrolled");
    }

    const after = blankRatio(sampleBlankRows(dlg, x0, x1, y, dy));
    if (after < before - MAX_BLANK_ROW_LOSS) {
      throw new Error(
        `issue-5882: scrolling filled in the gaps between rows: ${(before * 100).toFixed(0)}% of pixel ` +
          `rows were blank before, ${(after * 100).toFixed(0)}% after - rows are drawn over each other`,
      );
    }
    console.log(
      `issue-5882: OK itemDy=${g.itemDy} leftover=${best.leftover} scrollY=${g.scrollY} ` +
        `rows=${g.rows.length} blank rows ${(before * 100).toFixed(0)}% -> ${(after * 100).toFixed(0)}%`,
    );

    postMessage(dlg, WM_CLOSE, 0, 0);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
