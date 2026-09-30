// The sidebar's Thumbnails view: switching between Bookmarks and Thumbnails, a click
// going to a page, Shift / Ctrl click selecting several and dragging them to
// another place, undone with CmdUndo. Each page of the test PDF has a unique
// width (601, 602, ...) so the page order can be read back.
//
// Run: bun tests/sidebar-thumbnails.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { makePdf } from "./page-edit.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getFocusedHwnd,
  MK_CONTROL,
  MK_LBUTTON,
  MK_SHIFT,
  packCoords,
  postMessage,
  sendMessage,
  setCursorPos,
  readWindowDCRow,
  sleep,
  VK_DOWN,
  VK_NEXT,
  VK_TAB,
  VK_UP,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };
type Sidebar = {
  hwnd: number;
  visible: boolean;
  thumbnails: boolean;
  current: number;
  rendered: number;
  marked: string;
  labels: string;
  // the header's Bookmarks and Thumbnails labels
  labelRects: Rect[];
  rects: Map<number, Rect>;
  raw: string;
};

async function sidebar(client: ControlClient): Promise<Sidebar> {
  const res = await client.request(ControlCommand.TestSidebarThumbnails, []);
  const raw = String(res[1] ?? "");
  const m =
    /hwnd=(\d+) visible=(\d) thumbnails=(\d) count=\d+ current=(\d+) rendered=(\d+) marked=(\S*) ring=\d bookmarksLabel=(\d) thumbnailsLabel=(\d) labelRects=(\S*) rects=(\S*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !m) {
    throw new Error(`sidebar-thumbnails: TestSidebarThumbnails: ${raw}`);
  }
  const rects = new Map<number, Rect>();
  for (const part of m[10]!.split(";").filter(Boolean)) {
    const [page, coords] = part.split(":");
    const [x, y, dx, dy] = coords!.split(",").map(Number);
    rects.set(+page!, { x: x!, y: y!, dx: dx!, dy: dy! });
  }
  return {
    hwnd: +m[1]!,
    visible: m[2] === "1",
    thumbnails: m[3] === "1",
    current: +m[4]!,
    rendered: +m[5]!,
    marked: m[6]!,
    labels: `${m[7]}${m[8]}`,
    labelRects: m[9]!.split(";").map((part) => {
      const [x, y, dx, dy] = part.split(",").map(Number);
      return { x: x!, y: y!, dx: dx!, dy: dy! };
    }),
    rects,
    raw,
  };
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`sidebar-thumbnails: ${what}`);
    }
    await sleep(50);
  }
}

async function widths(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestPageEdit, ["", "", 0]);
  return /widths=([\d,]+)/.exec(String(res[1] ?? ""))?.[1] ?? "";
}

// a point inside page's thumbnail, fx / fy of the way across / down
function pointIn(s: Sidebar, page: number, fx: number, fy: number): { x: number; y: number } {
  const r = s.rects.get(page);
  if (!r || r.dx <= 0) {
    throw new Error(`sidebar-thumbnails: page ${page}'s thumbnail isn't visible: ${s.raw}`);
  }
  return { x: Math.round(r.x + r.dx * fx), y: Math.round(r.y + r.dy * fy) };
}

function mouse(hwnd: number, msg: number, mk: number, pt: { x: number; y: number }) {
  // SetCapture injects a WM_MOUSEMOVE at the real cursor: keep it where we are
  const screen = clientToScreen(hwnd, pt.x, pt.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(hwnd, msg, mk, packCoords(pt.x, pt.y));
}

function clickPt(hwnd: number, pt: { x: number; y: number }, mk = 0) {
  mouse(hwnd, WM_LBUTTONDOWN, MK_LBUTTON | mk, pt);
  mouse(hwnd, WM_LBUTTONUP, mk, pt);
}

function click(s: Sidebar, page: number, mk = 0) {
  clickPt(s.hwnd, pointIn(s, page, 0.5, 0.5), mk);
}

// the current page's frame (MkRgb(0, 120, 215)), as a COLORREF off the screen
function isFrameBlue(c: number): boolean {
  const r = c & 0xff;
  const g = (c >> 8) & 0xff;
  const b = (c >> 16) & 0xff;
  return r < 60 && g > 90 && g < 150 && b > 180;
}

// Whether the screen shows the current page's thumbnail framed on both sides.
// Read from the window DC, not PrintWindow (which paints afresh): a view switch
// that doesn't repaint leaves the bookmarks tree on screen
function frameOnScreen(s: Sidebar): boolean {
  const r = s.rects.get(s.current);
  if (!r || r.dx <= 0) {
    return false;
  }
  const edge = 8;
  const row = readWindowDCRow(s.hwnd, r.x - 2, r.y + Math.floor(r.dy / 2), r.dx + 4);
  return row.slice(0, edge).some(isFrameBlue) && row.slice(-edge).some(isFrameBlue);
}

// label 0 is Bookmarks, 1 is Thumbnails
function clickLabel(s: Sidebar, label: number) {
  const r = s.labelRects[label]!;
  if (r.dx <= 0) {
    throw new Error(`sidebar-thumbnails: header label ${label} isn't visible: ${s.raw}`);
  }
  clickPt(s.hwnd, { x: r.x + Math.floor(r.dx / 2), y: r.y + Math.floor(r.dy / 2) });
}

export async function testit(): Promise<void> {
  const dir = tmpPath("sidebar-thumbnails");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(4, 601, 3, 0), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // Bookmarks and Thumbnails switch the view, by command or by clicking the
    // header; Thumbnails again hides the sidebar
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await waitFor("Thumbnails didn't show", async () => (await sidebar(client)).thumbnails);
    let s = await sidebar(client);
    if (!s.visible || s.labels !== "11") {
      throw new Error(`sidebar-thumbnails: want both view labels: ${s.raw}`);
    }
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await waitFor("Bookmarks didn't replace Thumbnails", async () => {
      const b = await sidebar(client);
      return b.visible && !b.thumbnails;
    });
    clickLabel(await sidebar(client), 1);
    await waitFor("clicking Thumbnails didn't show Thumbnails", async () => (await sidebar(client)).thumbnails);
    await waitFor("clicking Thumbnails didn't repaint the sidebar", async () => frameOnScreen(await sidebar(client)));
    clickLabel(await sidebar(client), 0);
    await waitFor("clicking Bookmarks didn't show Bookmarks", async () => {
      const b = await sidebar(client);
      return b.visible && !b.thumbnails;
    });
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await waitFor("Thumbnails didn't show again", async () => (await sidebar(client)).thumbnails);
    await waitFor("no thumbnail rendered", async () => (await sidebar(client)).rendered > 0);

    // With the keyboard in the panel (ring drawn), Up / Down go through the
    // pages and every other key goes to the canvas. Tab moves to the canvas
    // (no ring) and, going around, back to the panel.
    s = await sidebar(client);
    const hwndPanel = s.hwnd;
    const ringOn = async () => /ring=1/.test((await sidebar(client)).raw);
    const panelFocused = async () => getFocusedHwnd(frame) === hwndPanel && (await ringOn());
    await waitFor("showing Thumbnails didn't focus them", panelFocused);
    const currentIs = (n: number) => async () => (await sidebar(client)).current === n;
    await waitFor("the document isn't on page 1", currentIs(1));
    // the thumbnails' own Up / Down also select the page; the canvas's don't
    const onPage = (n: number) => async () => {
      const t = await sidebar(client);
      return t.current === n && t.marked === `${n}`;
    };
    postMessage(hwndPanel, WM_KEYDOWN, VK_DOWN, 0);
    await waitFor("Down didn't go to thumbnail 2", onPage(2));
    postMessage(hwndPanel, WM_KEYDOWN, VK_UP, 0);
    await waitFor("Up didn't go to thumbnail 1", onPage(1));
    // the canvas turns one page; the thumbnails would jump a screenful
    postMessage(hwndPanel, WM_KEYDOWN, VK_NEXT, 0);
    await waitFor("Page Down didn't reach the canvas", currentIs(2));
    if (!(await panelFocused())) {
      throw new Error("sidebar-thumbnails: a key for the canvas took the focus from the panel");
    }
    postMessage(hwndPanel, WM_KEYDOWN, VK_TAB, 0);
    await waitFor("Tab didn't move to the canvas", async () => getFocusedHwnd(frame) === frame && !(await ringOn()));
    await waitFor("Tab didn't come back to the panel", async () => {
      if (await panelFocused()) {
        return true;
      }
      postMessage(getFocusedHwnd(frame), WM_KEYDOWN, VK_TAB, 0);
      await sleep(150);
      return false;
    });

    // a click goes to the page
    s = await sidebar(client);
    click(s, 3);
    await waitFor("a click on page 3 didn't go there", async () => (await sidebar(client)).current === 3);

    // Shift selects a range, Ctrl toggles one page
    s = await sidebar(client);
    click(s, 1);
    click(s, 2, MK_SHIFT);
    s = await sidebar(client);
    if (s.marked !== "1,2") {
      throw new Error(`sidebar-thumbnails: Shift click: want marked=1,2: ${s.raw}`);
    }
    click(s, 3, MK_CONTROL);
    click(s, 3, MK_CONTROL);
    s = await sidebar(client);
    if (s.marked !== "1,2") {
      throw new Error(`sidebar-thumbnails: Ctrl click twice: want marked=1,2: ${s.raw}`);
    }

    // pages 1 and 2 dragged behind page 3
    const dragBehind3 = (t: Sidebar) => {
      const from = pointIn(t, 1, 0.5, 0.5);
      const to = pointIn(t, 3, 0.9, 0.9);
      mouse(t.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, from);
      mouse(t.hwnd, WM_MOUSEMOVE, MK_LBUTTON, to);
      mouse(t.hwnd, WM_LBUTTONUP, 0, to);
    };
    // only in Edit PDF mode
    dragBehind3(s);
    await sleep(500);
    if ((await widths(client)) !== "601,602,603,604") {
      throw new Error("sidebar-thumbnails: pages moved outside Edit PDF mode");
    }
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);
    // the drag that didn't happen was a click on page 1: select 1 and 2 again
    s = await sidebar(client);
    click(s, 1);
    click(s, 2, MK_SHIFT);
    s = await sidebar(client);
    if (s.marked !== "1,2") {
      throw new Error(`sidebar-thumbnails: selecting pages again: want marked=1,2: ${s.raw}`);
    }
    dragBehind3(s);
    await waitFor("dragging didn't move the pages", async () => (await widths(client)) === "603,601,602,604");
    // the move reloads the bookmarks, which must stay hidden
    await waitFor("moving pages showed the bookmarks tree", async () => frameOnScreen(await sidebar(client)));
    s = await sidebar(client);
    if (s.marked !== "2,3") {
      throw new Error(`sidebar-thumbnails: the moved pages must stay selected: ${s.raw}`);
    }

    sendCommand(frame, cmdId("CmdUndo"));
    await waitFor("undo didn't restore the order", async () => (await widths(client)) === "601,602,603,604");

    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await waitFor("Thumbnails didn't hide the sidebar", async () => !(await sidebar(client)).visible);
    console.log("sidebar-thumbnails: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
