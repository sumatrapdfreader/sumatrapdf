// Resting the mouse on a toolbar button can open a drop-down, after the delay a
// tooltip takes, and the tooltip gives way to it. Right-click opens it at once
// if it is not already shown, and does not run the button's command. The Edit
// PDF toolbar's Save button uses it for the three ways to end an editing
// session, each row showing its keyboard shortcut; Save to a new PDF is no
// longer its own button. The Zoom In / Zoom Out buttons use it for the zoom
// levels, laid out as a pyramid: the widest row on top holding the middle of the
// list, each row below it the levels further out and the last the extremes. The
// levels above the middle sit on a slightly different background, so which way
// is bigger can be seen rather than read. The level in use is boxed, and the
// drop-down opens centred on the button.
//
// Run: bun tests/toolbar-hover-dropdown.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  captureWindowToPng,
  getWorkArea,
  clientToScreen,
  findTopWindow,
  getWindowRect,
  isWindowVisible,
  readWindowDCRow,
  packCoords,
  sendMessage,
  setCursorPos,
  setProcessDpiAware,
  sleep,
  MK_RBUTTON,
  WM_COMMAND,
  WM_MOUSEMOVE,
  WM_MOUSELEAVE,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import {
  clickAt,
  findChildByClass,
  killAndWait,
  launchControlled,
  parkCursorAway,
  sendCommand,
  sendCommandSync,
} from "./win-automation.ts";

const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const MENU_CLASS = "SumatraToolbarHoverMenu";

// what the zoom drop-down lists: the levels the zoom buttons step through
// (DisplayModel's defaultZoomLevels), smallest first, with the two fit modes
// where 100% is
const ZOOM_LEVELS = [
  "8.33%",
  "12.5%",
  "18%",
  "25%",
  "33.33%",
  "50%",
  "66.67%",
  "75%",
  "100%",
  "Fit Page",
  "Fit Width",
  "125%",
  "150%",
  "200%",
  "300%",
  "400%",
  "600%",
  "800%",
  "1000%",
  "1200%",
  "1600%",
  "2000%",
  "2400%",
  "3200%",
  "4800%",
  "6400%",
];

// ZoomLevels in the settings replaces the levels, for the buttons and the strip
// alike; the fit modes stay
const CUSTOM_ZOOM_LEVELS = [50, 75, 100, 150, 300];
const CUSTOM_ZOOM_STRIP = ["50%", "75%", "100%", "Fit Page", "Fit Width", "150%", "300%"];

// how many cells each row of the pyramid holds, top row first: the widest row
// a triangle of rows needs to hold them all, then one fewer each row down
const ZOOM_ROWS = [7, 6, 5, 4, 3, 1];
const CUSTOM_ZOOM_ROWS = [4, 3];

// a window away from the edges of the screen: a drop-down that would hang off
// the monitor is slid back on, which is right but takes it off centre
const WINDOW_POS = "1100x900@40x40";

type Btn = { cmd: number; hidden: boolean; x: number; y: number; dx: number; dy: number };

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 600 220 700] /C [1 0 0] >>",
  ]);
}

async function annotButtons(client: ControlClient): Promise<Btn[]> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const res: Btn[] = [];
  const re = /annotation-idx=\d+ cmd=(\d+) hidden=(\d) enabled=\d rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    const x = +m[3]!;
    const y = +m[4]!;
    res.push({ cmd: +m[1]!, hidden: m[2] === "1", x, y, dx: +m[5]! - x, dy: +m[6]! - y });
  }
  return res;
}

async function mainButtons(client: ControlClient): Promise<Btn[]> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const res: Btn[] = [];
  const re = /^idx=\d+ cmd=(\d+) hidden=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/gm;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    const x = +m[3]!;
    const y = +m[4]!;
    res.push({ cmd: +m[1]!, hidden: m[2] === "1", x, y, dx: +m[5]! - x, dy: +m[6]! - y });
  }
  return res;
}

async function waitAnnotButton(client: ControlClient, cmd: number, what: string): Promise<Btn> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const b = (await annotButtons(client)).find((v) => v.cmd === cmd && !v.hidden && v.dx > 0);
    if (b) {
      return b;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what}`);
    }
    await sleep(50);
  }
}

// the tooltip each main-row button is showing, empty for one that has none
async function mainButtonTips(client: ControlClient): Promise<Map<number, string>> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const re = /^idx=\d+ cmd=(\d+) hidden=\d rect=\S+ text=(.*)$/gm;
  const res = new Map<number, string>();
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    res.set(+m[1]!, m[2]!.trim());
  }
  return res;
}

async function annotTip(client: ControlClient, cmd: number): Promise<string | null> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const m = new RegExp(`annotation-idx=\\d+ cmd=${cmd} .* tip=(.*)$`, "m").exec(raw);
  return m ? m[1]!.trim() : null;
}

async function annotCount(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  return +(/annotations=(\d+)/.exec(raw)?.[1] ?? -1);
}

async function annotModified(client: ControlClient): Promise<boolean> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  return /undo canUndo=\d canRedo=\d modified=1/.test(raw);
}

async function waitAnnotCount(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const n = await annotCount(client);
    if (n === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what} (annotations=${n}, want ${want})`);
    }
    await sleep(50);
  }
}

// ng has no SUMATRA_VIRT_TOOLBAR and no SumatraToolbarHoverMenu. The buttons
// and the drop-down are gpui elements of the frame, driven with TestInput.
// gen changes when a new drop-down opens, not when the mouse crosses between
// Zoom In and Zoom Out.
type HoverBox = { x: number; y: number; x2: number; y2: number; bg: number; shade: number; gen: number };

type HoverView = { items: Item[]; box: HoverBox | null; right: Set<number> };

async function ngInput(client: ControlClient, args: (string | number)[]): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, args);
  const s = String(res[1] ?? "");
  if (!s.startsWith("OK")) {
    throw new Error(`toolbar-hover-dropdown: input ${s}`);
  }
}

async function readHover(client: ControlClient): Promise<HoverView> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const head = /^dropdown cmd=\d+ items=\d+ box=(-?\d+),(-?\d+),(-?\d+),(-?\d+) bg=(\d+) shade=(\d+) gen=(\d+)/m.exec(
    raw,
  );
  const box = head
    ? { x: +head[1]!, y: +head[2]!, x2: +head[3]!, y2: +head[4]!, bg: +head[5]!, shade: +head[6]!, gen: +head[7]! }
    : null;
  const items = dropdownItemsFrom(raw);
  const right = new Set<number>();
  let m: RegExpExecArray | null;
  const re = /^dropdown-right idx=(\d+)/gm;
  while ((m = re.exec(raw)) !== null) {
    right.add(+m[1]!);
  }
  const dipRe = /^dropdown-dip idx=(\d+) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/gm;
  while ((m = dipRe.exec(raw)) !== null) {
    const it = items[+m[1]!];
    if (it) {
      it.dx = +m[2]!;
      it.dy = +m[3]!;
      it.dx2 = +m[4]!;
      it.dy2 = +m[5]!;
    }
  }
  return { items, box, right };
}

async function waitMenu(client: ControlClient, pid: number, want: boolean, what: string): Promise<number> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const h = await menuShowing(client, pid);
    if ((h !== 0) === want) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what}`);
    }
    await sleep(50);
  }
}

async function menuShowing(client: ControlClient, pid: number): Promise<number> {
  if (USE_NG) {
    const view = await readHover(client);
    return view.box && view.box.x2 > view.box.x ? view.box.gen : 0;
  }
  const h = findTopWindow(pid, MENU_CLASS);
  return h !== 0 && isWindowVisible(h) ? h : 0;
}

// A hwnd read before an await can already be a different drop-down: the cursor
// gets yanked, the 150ms timer closes it, and resting on the button opens
// another. Click only when the row and the window are still the one just read.
async function clickMenuItem(
  client: ControlClient,
  frame: number,
  pid: number,
  cmd: number,
  what: string,
): Promise<void> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  let detail = "";
  let clicked = false;
  for (;;) {
    const h = await menuShowing(client, pid);
    if (clicked && h === 0) {
      return;
    }
    if (h === 0) {
      detail = "closed before the click";
    } else {
      const view = USE_NG ? await readHover(client) : null;
      const it = (view ? view.items : await dropdownItems(client)).find((x) => x.cmd === cmd);
      if (!it) {
        detail = "row not in the drop-down";
      } else if ((await menuShowing(client, pid)) === h) {
        if (USE_NG) {
          const x = Math.floor((it.dx + it.dx2) / 2);
          const y = Math.floor((it.dy + it.dy2) / 2);
          await ngInput(client, ["click", x, y, 0, 0]);
          detail = `${it.text} @${x},${y}`;
        } else {
          const r = getWindowRect(h);
          const x = Math.floor((it.x + it.x2) / 2) - r.left;
          const y = Math.floor((it.y + it.y2) / 2) - r.top;
          await clickAt(h, x, y, 0);
          detail = `${it.text} @${x},${y}`;
        }
        clicked = true;
        if ((await menuShowing(client, pid)) === 0) {
          return;
        }
      }
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what} (${detail})`);
    }
    await sleep(40);
  }
}

// rest the mouse on a toolbar button: the real cursor has to be there (the
// drop-down reads it) and the toolbar has to see a move
async function hoverToolbar(client: ControlClient, toolbar: number, x: number, y: number): Promise<void> {
  if (USE_NG) {
    await ngInput(client, ["move", x, y, 0, 0]);
    return;
  }
  const s = clientToScreen(toolbar, x, y);
  setCursorPos(s.x, s.y);
  sendMessage(toolbar, WM_MOUSEMOVE, 0, packCoords(x, y));
}

// right-click a toolbar button. The cursor has to be there so the drop-down
// does not close itself, but the toolbar is not sent a move: that would start
// the hover timer, and this is the check that right-click does not wait for it
async function rightClickToolbar(client: ControlClient, toolbar: number, x: number, y: number): Promise<void> {
  if (USE_NG) {
    // button 1 is the right button. The click also moves, which arms the hover
    // timer; the button-up opens the drop-down without waiting for it.
    await ngInput(client, ["click", x, y, 1, 0]);
    return;
  }
  const s = clientToScreen(toolbar, x, y);
  setCursorPos(s.x, s.y);
  const lp = packCoords(x, y);
  sendMessage(toolbar, WM_RBUTTONDOWN, MK_RBUTTON, lp);
  sendMessage(toolbar, WM_RBUTTONUP, 0, lp);
}

// keep resting the mouse on a button until its drop-down is up: something else
// on the machine can yank the cursor away, and the drop-down reads where it
// actually is, so a single move is not enough to rely on
async function hoverUntilMenu(
  client: ControlClient,
  toolbar: number,
  pid: number,
  x: number,
  y: number,
  what: string,
): Promise<number> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  for (;;) {
    await hoverToolbar(client, toolbar, x, y);
    const h = await menuShowing(client, pid);
    if (h !== 0) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what}`);
    }
    await sleep(100);
  }
}

function toolbarOf(frame: number): number {
  if (USE_NG) {
    return frame;
  }
  const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
  if (!toolbar) {
    throw new Error("toolbar-hover-dropdown: no toolbar");
  }
  return toolbar;
}

async function zoomLabel(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "");
  return /zoom=(.+)$/m.exec(raw)?.[1]?.trim() ?? "";
}

async function waitZoom(client: ControlClient, want: string, what: string): Promise<void> {
  const deadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const z = await zoomLabel(client);
    if (z === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`toolbar-hover-dropdown: ${what} (zoom=${z}, want ${want})`);
    }
    await sleep(50);
  }
}

type Item = {
  cmd: number;
  current: boolean;
  x: number;
  y: number;
  x2: number;
  y2: number;
  text: string;
  // ng: the same rect in frame dips, which is where TestInput clicks
  dx: number;
  dy: number;
  dx2: number;
  dy2: number;
};

function dropdownItemsFrom(raw: string): Item[] {
  const re = /^dropdown-item idx=\d+ cmd=(\d+) current=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) text=(.*)$/gm;
  const res: Item[] = [];
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    res.push({
      cmd: +m[1]!,
      current: m[2] === "1",
      x: +m[3]!,
      y: +m[4]!,
      x2: +m[5]!,
      y2: +m[6]!,
      text: m[7]!.trim(),
      dx: 0,
      dy: 0,
      dx2: 0,
      dy2: 0,
    });
  }
  return res;
}

// what the drop-down that is up is showing, in screen coordinates
async function dropdownItems(client: ControlClient): Promise<Item[]> {
  if (USE_NG) {
    return (await readHover(client)).items;
  }
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  return dropdownItemsFrom(raw);
}

// the cells grouped into rows, the top row first. The dump lists them smallest
// level first whatever row they landed in, and that order is kept within a row
function rowsOf(items: Item[]): Item[][] {
  const byY = new Map<number, Item[]>();
  for (const it of items) {
    const row = byY.get(it.y);
    if (row) {
      row.push(it);
    } else {
      byY.set(it.y, [it]);
    }
  }
  return [...byY.entries()].sort((a, b) => a[0] - b[0]).map((e) => e[1]);
}

// A pyramid, not one long row: the top row holds the middle of the list, so
// the levels nearest the one in use are the shortest trip from the button, and
// each row below holds what surrounds it, down to the extremes. Rows are
// centred on one another and inside a row the levels run smallest to largest.
function checkPyramid(items: Item[], want: number[], levels: string[]): void {
  const rows = rowsOf(items);
  const shape = rows.map((r) => r.length);
  if (shape.join() !== want.join()) {
    throw new Error(`toolbar-hover-dropdown: the rows hold [${shape.join()}], want [${want.join()}]`);
  }
  const mid = Math.floor((levels.length - want[0]!) / 2);
  const top = rows[0]!.map((it) => it.text).join();
  if (top !== levels.slice(mid, mid + want[0]!).join()) {
    throw new Error(`toolbar-hover-dropdown: the top row is [${top}], want the middle of the list`);
  }
  const last = rows[rows.length - 1]!;
  if (last[last.length - 1]!.text !== levels[levels.length - 1]) {
    throw new Error(`toolbar-hover-dropdown: the bottom row is [${last.map((it) => it.text).join()}], want the ends`);
  }
  const centreOf = (r: Item[]) => Math.floor((r[0]!.x + r[r.length - 1]!.x2) / 2);
  const centre = centreOf(rows[0]!);
  for (let i = 0; i < rows.length; i++) {
    const r = rows[i]!;
    const texts = r.map((it) => it.text).join();
    if (Math.abs(centreOf(r) - centre) > 12) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] is not centred under the one above it`);
    }
    for (let j = 1; j < r.length; j++) {
      if (r[j]!.x < r[j - 1]!.x2 - 1) {
        throw new Error(`toolbar-hover-dropdown: the row [${texts}] does not run smallest to largest`);
      }
    }
    if (i > 0 && r[0]!.y < rows[i - 1]![0]!.y2) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] overlaps the one above it`);
    }
  }
}

// The right half of the pyramid - the levels above the middle - is on its own
// background, which runs to the right edge of the drop-down so the empty space
// beside a short row is covered too. The rows are staggered, so the two grounds
// meet along a staircase rather than one straight edge.
// a COLORREF (0x00bbggrr) as r, g, b
function channels(c: number): number[] {
  return [c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff];
}

function checkRightHalfShading(items: Item[], levels: string[], menu: number): { bg: number; shade: number } {
  const wr = getWindowRect(menu);
  const dx = wr.right - wr.left;
  const rows = rowsOf(items);
  const topLen = rows[0]!.length;
  // where the top row splits; past that point, in every row, is the larger side
  const rightFrom = Math.floor((levels.length - topLen) / 2) + Math.floor(topLen / 2);
  const splits: number[] = [];
  let bg = -1;
  let shade = -1;
  for (const r of rows) {
    const y = Math.floor((r[0]!.y + r[0]!.y2) / 2) - wr.top;
    const run = readWindowDCRow(menu, 0, y, dx);
    const texts = r.map((it) => it.text).join();
    // a row of nothing but smaller levels still has the space past its end on
    // the larger side
    const first = r.find((it) => levels.indexOf(it.text) >= rightFrom);
    const split = (first ? first.x : r[r.length - 1]!.x2) - wr.left;
    splits.push(split);
    // 3px either side of the split: inside a cell's padding, clear of its text
    const left = run[split - 3]!;
    const right = run[split + 3]!;
    if (left === right) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] is one ground either side of x=${split}`);
    }
    if (bg < 0) {
      bg = left;
      shade = right;
    }
    if (left !== bg || right !== shade) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] is not the same two grounds as the rows above`);
    }
    if (run[dx - 3] !== shade) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] leaves the space past its end unshaded`);
    }
    if (run[2] !== bg) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] shades the space before its start`);
    }
  }
  if (new Set(splits).size < 2) {
    throw new Error(`toolbar-hover-dropdown: the two grounds meet along a straight line at ${splits.join()}`);
  }
  return { bg, shade };
}

// ng paints the pyramid in the frame, and PrintWindow does not see that, so the
// half is the flag on each cell and the two grounds are the colours it uses.
function checkRightHalfShadingNg(
  items: Item[],
  levels: string[],
  box: HoverBox,
  right: Set<number>,
): { bg: number; shade: number } {
  const rows = rowsOf(items);
  const topLen = rows[0]!.length;
  const rightFrom = Math.floor((levels.length - topLen) / 2) + Math.floor(topLen / 2);
  const splits: number[] = [];
  let shortRow = false;
  for (const r of rows) {
    const texts = r.map((it) => it.text).join();
    const first = r.find((it) => levels.indexOf(it.text) >= rightFrom);
    const split = first ? first.x : r[r.length - 1]!.x2;
    splits.push(split);
    for (const it of r) {
      const onRight = levels.indexOf(it.text) >= rightFrom;
      if (onRight !== right.has(items.indexOf(it))) {
        throw new Error(`toolbar-hover-dropdown: the row [${texts}] shades the wrong half`);
      }
    }
    // the plain ground owns the left edge. A right-half cell sitting on the
    // border means the shade starts there instead of at the staircase. A short
    // row's first cell can be on the larger side and still leave plain ground
    // in the filler before it.
    if (right.has(items.indexOf(r[0]!)) && r[0]!.x < box.x + 4) {
      throw new Error(`toolbar-hover-dropdown: the row [${texts}] shades the space before its start`);
    }
    if (r[r.length - 1]!.x2 < box.x2 - 4) {
      shortRow = true;
    }
  }
  if (!shortRow) {
    throw new Error("toolbar-hover-dropdown: no row leaves shaded space past its end");
  }
  if (new Set(splits).size < 2) {
    throw new Error(`toolbar-hover-dropdown: the two grounds meet along a straight line at ${splits.join()}`);
  }
  return { bg: box.bg, shade: box.shade };
}

// the level in use is the one boxed, and only it
function checkCurrentBoxed(items: Item[], want: string): void {
  const current = items.filter((it) => it.current);
  if (current.length !== 1 || current[0]!.text !== want) {
    throw new Error(
      `toolbar-hover-dropdown: want only ${want} marked as the current zoom, got ` +
        `[${current.map((it) => it.text).join()}] of [${items.map((it) => it.text).join()}]`,
    );
  }
}

// The close timer is 150ms and it reads the real cursor. Hold the cursor on
// the button and re-read until the level is boxed: one read after a sleep
// sees an empty strip when the cursor was yanked off.
async function waitCurrentZoom(
  client: ControlClient,
  toolbar: number,
  pid: number,
  x: number,
  y: number,
  want: string,
): Promise<{ menu: number; items: Item[] }> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last: Item[] = [];
  for (;;) {
    await hoverToolbar(client, toolbar, x, y);
    const menu = await menuShowing(client, pid);
    if (menu !== 0) {
      last = await dropdownItems(client);
      const current = last.filter((it) => it.current);
      if (current.length === 1 && current[0]!.text === want && (await menuShowing(client, pid)) === menu) {
        await hoverToolbar(client, toolbar, x, y);
        return { menu, items: last };
      }
    }
    if (Date.now() > deadline) {
      checkCurrentBoxed(last, want);
      throw new Error(`toolbar-hover-dropdown: want only ${want} marked as the current zoom`);
    }
    await sleep(40);
  }
}

// the drop-down hangs off the middle of the button it belongs to, so it opens
// around where the mouse already is whatever it is showing. One that would run
// off the monitor is slid back on, which is right and takes it off centre
function checkCentredOnButton(left: number, right: number, btnCentreX: number): void {
  const centre = Math.floor((left + right) / 2);
  if (Math.abs(centre - btnCentreX) <= 4) {
    return;
  }
  const wa = getWorkArea();
  const clamped = left <= wa.left + 1 || right >= wa.right - 1;
  if (!clamped) {
    throw new Error(
      `toolbar-hover-dropdown: the drop-down is centred at x=${centre}, not on the button at x=${btnCentreX}`,
    );
  }
  // slid back on: it went as far as it could towards the button
  const wantLeft = left <= wa.left + 1 ? wa.left : wa.right - (right - left);
  if (Math.abs(left - wantLeft) > 4) {
    throw new Error("toolbar-hover-dropdown: the drop-down is neither on the button nor against the screen edge");
  }
}

async function menuBounds(
  client: ControlClient,
  menu: number,
): Promise<{ left: number; top: number; right: number; bottom: number }> {
  if (USE_NG) {
    const box = (await readHover(client)).box;
    if (!box || box.gen !== menu) {
      throw new Error("toolbar-hover-dropdown: the drop-down closed");
    }
    return { left: box.x, top: box.y, right: box.x2, bottom: box.y2 };
  }
  const r = getWindowRect(menu);
  return { left: r.left, top: r.top, right: r.right, bottom: r.bottom };
}

async function shadingOf(
  client: ControlClient,
  items: Item[],
  levels: string[],
  menu: number,
): Promise<{ bg: number; shade: number }> {
  if (USE_NG) {
    const view = await readHover(client);
    if (!view.box) {
      throw new Error("toolbar-hover-dropdown: the drop-down closed before its shading could be read");
    }
    return checkRightHalfShadingNg(view.items.length ? view.items : items, levels, view.box, view.right);
  }
  return checkRightHalfShading(items, levels, menu);
}

async function clickDropCell(client: ControlClient, menu: number, it: Item): Promise<void> {
  if (USE_NG) {
    const x = Math.floor((it.dx + it.dx2) / 2);
    const y = Math.floor((it.dy + it.dy2) / 2);
    await ngInput(client, ["click", x, y, 0, 0]);
    return;
  }
  const r = getWindowRect(menu);
  await clickAt(menu, Math.floor((it.x + it.x2) / 2) - r.left, Math.floor((it.y + it.y2) / 2) - r.top, 0);
}

// a second instance, this one told to use its own zoom levels and a dark theme:
// the cue is a shade off whatever the background is, not a fixed grey
async function checkCustomZoomLevels(dir: string, pdf: string): Promise<void> {
  const appdata = join(dir, "appdata-custom");
  mkdirSync(appdata);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n" +
      `Theme = Dark\nZoomLevels = ${CUSTOM_ZOOM_LEVELS.join(" ")}\n`,
  );
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-window-pos",
    WINDOW_POS,
    "-zoom",
    "100",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const toolbar = toolbarOf(frame);
    const zoomIn = (await mainButtons(client)).find((b) => b.cmd === cmdId("CmdZoomIn") && !b.hidden && b.dx > 0);
    if (!zoomIn) {
      throw new Error("toolbar-hover-dropdown: no Zoom In button");
    }
    const zx = zoomIn.x + Math.floor(zoomIn.dx / 2);
    const zy = zoomIn.y + Math.floor(zoomIn.dy / 2);
    await rightClickToolbar(client, toolbar, zx, zy);
    const opened = await waitMenu(client, proc.pid!, true, "right-clicking Zoom In did not open the custom drop-down");
    await sleep(200);
    const items = await dropdownItems(client);
    if (items.map((it) => it.text).join() !== CUSTOM_ZOOM_STRIP.join()) {
      throw new Error(`toolbar-hover-dropdown: custom ZoomLevels give [${items.map((it) => it.text).join()}]`);
    }
    checkPyramid(items, CUSTOM_ZOOM_ROWS, CUSTOM_ZOOM_STRIP);
    const dark = await shadingOf(client, items, CUSTOM_ZOOM_STRIP, opened);
    if (channels(dark.bg).some((c) => c > 128)) {
      throw new Error(`toolbar-hover-dropdown: the dark theme drop-down is not dark (${dark.bg})`);
    }
    // lighter than what it sits on, the way it is darker in a light theme
    if (channels(dark.shade).every((c, i) => c <= channels(dark.bg)[i]!)) {
      throw new Error("toolbar-hover-dropdown: the dark theme shades the larger half darker, not lighter");
    }
    checkCurrentBoxed(items, "100%");
    const openedBox = await menuBounds(client, opened);
    checkCentredOnButton(openedBox.left, openedBox.right, clientToScreen(toolbar, zx, zy).x);

    // and they are real commands, not just labels
    const menu = await hoverUntilMenu(client, toolbar, proc.pid!, zx, zy, "the zoom drop-down closed");
    const cell150 = items.find((it) => it.text === "150%")!;
    await clickDropCell(client, menu, cell150);
    await waitZoom(client, "150", "clicking a custom level did not zoom to it");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const dir = tmpPath("toolbar-hover-dropdown");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-window-pos",
    WINDOW_POS,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const pid = proc.pid!;
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));

    const save = await waitAnnotButton(client, cmdId("CmdSaveAnnotations"), "no Save button on the Edit PDF toolbar");
    if ((await annotButtons(client)).some((b) => b.cmd === cmdId("CmdSaveAnnotationsNewFile"))) {
      throw new Error("toolbar-hover-dropdown: Save to a new PDF is still its own toolbar button");
    }

    const toolbar = toolbarOf(frame);
    if (await menuShowing(client, pid)) {
      throw new Error("toolbar-hover-dropdown: the drop-down was up before anything was hovered");
    }

    // something to save, so the rows are live
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    await waitAnnotCount(client, 2, "could not create an annotation to save");

    // creating it may have relaid out the row, so re-read where Save sits
    const save2 = await waitAnnotButton(client, cmdId("CmdSaveAnnotations"), "Save button vanished");
    const cx = save2.x + Math.floor(save2.dx / 2);
    const cy = save2.y + Math.floor(save2.dy / 2);
    const menu = await hoverUntilMenu(client, toolbar, pid, cx, cy, "resting on Save did not open the drop-down");
    await sleep(200);
    if (!USE_NG) {
      captureWindowToPng(menu, join(dir, "dropdown.png"));
    }

    // directly under the button (it slides left when its width would run off
    // the right edge of the screen, so only the button's centre has to be in it)
    const mr = await menuBounds(client, menu);
    const below = clientToScreen(toolbar, save2.x, save2.y + save2.dy);
    const centreX = below.x + Math.floor(save2.dx / 2);
    if (Math.abs(mr.top - below.y) > 2 || mr.left > centreX || mr.right < centreX) {
      throw new Error(
        `toolbar-hover-dropdown: drop-down is not under the button ${JSON.stringify(mr)} vs ${JSON.stringify(below)}`,
      );
    }
    const dy = mr.bottom - mr.top;
    if (mr.right - mr.left < save2.dx || dy < save2.dy) {
      throw new Error(`toolbar-hover-dropdown: drop-down looks too small ${JSON.stringify(mr)}`);
    }

    // the button gives up its tooltip while the drop-down is up, so nothing
    // brings the bubble back over it
    if (await annotTip(client, cmdId("CmdSaveAnnotations"))) {
      throw new Error("toolbar-hover-dropdown: the button kept its tooltip under the drop-down");
    }

    // the third row is Discard changes: clicking it runs that command, which
    // proves there are three rows and that a click reaches the right one
    const discard = cmdId("CmdDiscardChanges");
    const saveRows = await dropdownItems(client);
    if (saveRows.length !== 3 || saveRows[2]!.cmd !== discard) {
      throw new Error(
        `toolbar-hover-dropdown: expected Discard changes on the third row, got [${saveRows.map((it) => it.text).join(" | ")}]`,
      );
    }
    await clickMenuItem(client, frame, pid, discard, "clicking a row did not close the drop-down");
    await waitAnnotCount(client, 1, "the third row did not discard the changes");
    if (!(await annotTip(client, cmdId("CmdSaveAnnotations")))) {
      throw new Error("toolbar-hover-dropdown: the button did not get its tooltip back");
    }

    // moving the mouse off it closes it too. A move message at a toolbar point
    // can hit another button and leave that button's drop-down up; a leave
    // follows the real cursor, which has to be off the frame.
    await hoverUntilMenu(client, toolbar, pid, cx, cy, "the drop-down did not open a second time");
    const awayDeadline = Date.now() + 6000 * SLOW_BUILD_FACTOR;
    for (;;) {
      const menu = await menuShowing(client, pid);
      if (menu === 0) {
        break;
      }
      if (USE_NG) {
        // off the toolbar, on the page. The close grace is 150ms.
        await ngInput(client, ["move", 40, 500, 0, 0]);
      } else {
        parkCursorAway([getWindowRect(frame), getWindowRect(menu)]);
        sendMessage(menu, WM_MOUSELEAVE, 0, 0);
        sendMessage(toolbar, WM_MOUSELEAVE, 0, 0);
      }
      const sliceEnd = Date.now() + 400;
      let gone = false;
      while (Date.now() < sliceEnd) {
        if ((await menuShowing(client, pid)) === 0) {
          gone = true;
          break;
        }
        await sleep(50);
      }
      if (gone) {
        break;
      }
      if (Date.now() > awayDeadline) {
        throw new Error("toolbar-hover-dropdown: moving the mouse away did not close the drop-down");
      }
    }

    // clicking the Save icon itself must close the drop-down: the three rows
    // end the session, and after a save they no longer apply
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(180, 340));
    await waitAnnotCount(client, 2, "could not create an annotation to save from the icon");
    const save3 = await waitAnnotButton(
      client,
      cmdId("CmdSaveAnnotations"),
      "Save button vanished before the icon click",
    );
    const sx = save3.x + Math.floor(save3.dx / 2);
    const sy = save3.y + Math.floor(save3.dy / 2);
    await rightClickToolbar(client, toolbar, sx, sy);
    await waitMenu(client, pid, true, "right-clicking Save did not open the drop-down before the icon click");
    if (USE_NG) {
      await ngInput(client, ["click", sx, sy, 0, 0]);
    } else {
      await clickAt(toolbar, sx, sy, 0);
    }
    await waitMenu(client, pid, false, "clicking the Save icon did not close the drop-down");
    const stayClosedUntil = Date.now() + 1500;
    while (Date.now() < stayClosedUntil) {
      await hoverToolbar(client, toolbar, sx, sy);
      if (await menuShowing(client, pid)) {
        throw new Error("toolbar-hover-dropdown: the drop-down came back while still on Save after the icon click");
      }
      await sleep(100);
    }
    if (await annotModified(client)) {
      throw new Error("toolbar-hover-dropdown: clicking the Save icon did not save");
    }

    // --- the zoom buttons list the zoom levels
    sendCommand(frame, cmdId("CmdZoom100"));
    await waitZoom(client, "100", "could not set the zoom to 100%");

    const zoomIn = (await mainButtons(client)).find((b) => b.cmd === cmdId("CmdZoomIn") && !b.hidden && b.dx > 0);
    if (!zoomIn) {
      throw new Error("toolbar-hover-dropdown: no Zoom In button");
    }
    const zx = zoomIn.x + Math.floor(zoomIn.dx / 2);
    const zy = zoomIn.y + Math.floor(zoomIn.dy / 2);
    const btnCentreX = clientToScreen(toolbar, zx, zy).x;

    // right-click opens it at once and does not run Zoom In. ng lays the strip
    // out on the next frame, which is still far short of the hover delay.
    await rightClickToolbar(client, toolbar, zx, zy);
    let zoomMenu = 0;
    {
      const openedBy = Date.now() + 250;
      for (;;) {
        zoomMenu = await menuShowing(client, pid);
        if (zoomMenu !== 0 || Date.now() > openedBy) {
          break;
        }
        await sleep(20);
      }
    }
    if (zoomMenu === 0) {
      throw new Error("toolbar-hover-dropdown: right-click on Zoom In did not open the drop-down");
    }
    if ((await zoomLabel(client)) !== "100") {
      throw new Error("toolbar-hover-dropdown: right-click on Zoom In ran Zoom In");
    }
    const afterRightClick = zoomMenu;
    await rightClickToolbar(client, toolbar, zx, zy);
    zoomMenu = await menuShowing(client, pid);
    if (zoomMenu !== afterRightClick) {
      throw new Error("toolbar-hover-dropdown: right-click recreated a drop-down that was already shown");
    }

    zoomMenu = await hoverUntilMenu(client, toolbar, pid, zx, zy, "resting on Zoom In did not open the drop-down");
    await sleep(200);
    if (!USE_NG) {
      captureWindowToPng(zoomMenu, join(dir, "zoom-dropdown.png"));
    }

    let items = await dropdownItems(client);
    const texts = items.map((it) => it.text);
    if (texts.join() !== ZOOM_LEVELS.join()) {
      throw new Error(`toolbar-hover-dropdown: the zoom levels are [${texts.join()}]`);
    }
    checkPyramid(items, ZOOM_ROWS, ZOOM_LEVELS);
    const zr = await menuBounds(client, zoomMenu);
    // and the pyramid is what keeps it narrow: all 26 in a row would be wider
    // than the window the toolbar is in
    if (zr.right - zr.left > 600) {
      throw new Error(`toolbar-hover-dropdown: the zoom drop-down is too wide ${JSON.stringify(zr)}`);
    }
    checkCurrentBoxed(items, "100%");
    checkCentredOnButton(zr.left, zr.right, btnCentreX);
    const zoomMenuRect = JSON.stringify(zr);

    const grounds = await shadingOf(client, items, ZOOM_LEVELS, zoomMenu);

    // clicking a level zooms straight to it. 300% is one of the levels the Zoom
    // menu has no command for, so it is also the check that those levels get a
    // command of their own
    const cell300 = items.find((it) => it.text === "300%")!;
    // and it is a cue rather than a highlight: a few units off the plain
    // ground, where a cell lit up under the mouse is 20 units off it
    if (USE_NG) {
      const view = await readHover(client);
      if (!view.right.has(items.indexOf(cell300))) {
        throw new Error("toolbar-hover-dropdown: 300% is not on the shaded half");
      }
    } else {
      const cellX = cell300.x - zr.left + 3;
      const cellY = Math.floor((cell300.y + cell300.y2) / 2) - zr.top;
      if (readWindowDCRow(zoomMenu, cellX, cellY, 1)[0] !== grounds.shade) {
        throw new Error("toolbar-hover-dropdown: 300% is not on the shaded half");
      }
    }
    const step = Math.max(...channels(grounds.bg).map((c, i) => Math.abs(c - channels(grounds.shade)[i]!)));
    if (step < 3 || step > 15) {
      throw new Error(`toolbar-hover-dropdown: the shading is ${step} units off the background, want a few`);
    }

    await clickDropCell(client, zoomMenu, cell300);
    await waitMenu(client, pid, false, "clicking a zoom level did not close the drop-down");
    await waitZoom(client, "300", "clicking the 300% cell did not zoom to it");

    // and now that level is the one boxed. The drop-down itself opens exactly
    // where it did before: it hangs off the button, not off the zoom
    {
      await rightClickToolbar(client, toolbar, zx, zy);
      const boxed = await waitCurrentZoom(client, toolbar, pid, zx, zy, "300%");
      zoomMenu = boxed.menu;
      items = boxed.items;
    }
    const again = await menuBounds(client, zoomMenu);
    checkCentredOnButton(again.left, again.right, btnCentreX);
    if (JSON.stringify(again) !== zoomMenuRect) {
      throw new Error("toolbar-hover-dropdown: the drop-down opened somewhere else once the zoom had changed");
    }

    // stepping the zoom with the button the strip belongs to moves the box to
    // the level it lands on, without the strip itself moving out from under
    // the mouse mid-click
    const before = items.map((it) => `${it.text}@${it.x}`).join();
    sendCommand(frame, cmdId("CmdZoomIn"));
    await waitZoom(client, "400", "the Zoom In button did not step the zoom");
    items = await dropdownItems(client);
    const after = items.map((it) => `${it.text}@${it.x}`).join();
    if (after !== before) {
      throw new Error("toolbar-hover-dropdown: the strip moved when the zoom was stepped");
    }
    let boxed = items.filter((it) => it.current).map((it) => it.text);
    if (boxed.join() !== "400%") {
      throw new Error(`toolbar-hover-dropdown: the box did not follow the zoom, it is on [${boxed.join()}]`);
    }

    // a zoom that is none of them leaves nothing boxed
    sendCommandSync(frame, cmdId("CmdZoomFitContent"));
    await waitZoom(client, "fit content", "could not set Fit Content zoom");
    boxed = (await dropdownItems(client)).filter((it) => it.current).map((it) => it.text);
    if (boxed.length !== 0) {
      throw new Error(`toolbar-hover-dropdown: a zoom off the list still boxes [${boxed.join()}]`);
    }
    sendCommand(frame, cmdId("CmdZoom200"));
    await waitZoom(client, "200", "could not put the zoom back on a listed level");

    // the two zoom buttons share the strip: crossing from one to the other
    // leaves it exactly where it is rather than sliding it under the other
    // button, and it does not close and open again on the way
    const zoomOut = (await mainButtons(client)).find((b) => b.cmd === cmdId("CmdZoomOut") && !b.hidden && b.dx > 0);
    if (!zoomOut) {
      throw new Error("toolbar-hover-dropdown: no Zoom Out button");
    }
    const ox = zoomOut.x + Math.floor(zoomOut.dx / 2);
    const oy = zoomOut.y + Math.floor(zoomOut.dy / 2);
    // zoomMenu was captured when the strip opened. The awaits since then are
    // long enough for a yanked cursor to close that window and open another
    // in the same place; the cross has to be judged against the one up now.
    if ((await menuShowing(client, pid)) !== zoomMenu) {
      zoomMenu = await hoverUntilMenu(
        client,
        toolbar,
        pid,
        zx,
        zy,
        "the zoom drop-down was not up before crossing to Zoom Out",
      );
    }
    const wasAt = (await dropdownItems(client)).map((it) => `${it.text}@${it.x}`).join();
    const live = await menuShowing(client, pid);
    if (live !== 0) {
      zoomMenu = live;
    }
    const wasRect = JSON.stringify(await menuBounds(client, zoomMenu));
    // the close timer is 150ms and it reads the real cursor. Keep the cursor
    // on Zoom Out across that window: one move and a long sleep loses it on a
    // busy machine, and the strip then closes
    const deadline = Date.now() + 400 * SLOW_BUILD_FACTOR;
    let stillUp = 0;
    for (;;) {
      await hoverToolbar(client, toolbar, ox, oy);
      stillUp = await menuShowing(client, pid);
      if (stillUp !== zoomMenu) {
        throw new Error(
          `toolbar-hover-dropdown: moving onto Zoom Out did not keep the same drop-down (hwnd ${stillUp} vs ${zoomMenu})`,
        );
      }
      if (Date.now() >= deadline) {
        break;
      }
      await sleep(40);
    }
    items = await dropdownItems(client);
    if (
      items.map((it) => `${it.text}@${it.x}`).join() !== wasAt ||
      JSON.stringify(await menuBounds(client, stillUp)) !== wasRect
    ) {
      throw new Error("toolbar-hover-dropdown: the strip moved when the mouse crossed to Zoom Out");
    }

    // and Zoom Out's own tooltip is the one suppressed now
    const tips = await mainButtonTips(client);
    if (tips.get(cmdId("CmdZoomOut")) || !tips.get(cmdId("CmdZoomIn"))) {
      throw new Error("toolbar-hover-dropdown: the tooltips did not follow the mouse to Zoom Out");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  await checkCustomZoomLevels(dir, pdf);

  console.log("toolbar-hover-dropdown: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
