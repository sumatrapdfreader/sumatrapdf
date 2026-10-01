// The sidebar's panels and its Thumbnails view: which panel a command opens a
// view in, a view icon switching (or swapping) panel views, a click going to a
// page and a drag not moving pages (that is Merge PDF's job). Each page of the
// test PDF has a unique width (601, 602, ...) so the page order can be read back.
//
// Run: bun tests/sidebar-thumbnails.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getFocusedHwnd,
  MK_LBUTTON,
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
import { killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };
type View = "bookmarks" | "thumbnails" | "favorites";
const views: View[] = ["bookmarks", "thumbnails", "favorites"];
type Panel = {
  hwnd: number;
  visible: boolean;
  view: View;
  // per view icon, in order Bookmarks, Thumbnails, Favorites
  enabled: string;
  selected: string;
  iconRects: Rect[];
};
type Sidebar = {
  // the panel showing the thumbnails, else the top one
  hwnd: number;
  thumbnails: boolean;
  current: number;
  rendered: number;
  top: Panel;
  bottom: Panel;
  rects: Map<number, Rect>;
  raw: string;
};

function parseRect(s: string): Rect {
  const [x, y, dx, dy] = s.split(",").map(Number);
  return { x: x!, y: y!, dx: dx!, dy: dy! };
}

// e.g. 1234,1,thumbnails,111,010:2,2,22,22;26,2,22,22;50,2,22,22
function parsePanel(s: string): Panel {
  const [head, rects] = s.split(":");
  const [hwnd, visible, view, enabled, selected] = head!.split(",");
  return {
    hwnd: +hwnd!,
    visible: visible === "1",
    view: view as View,
    enabled: enabled!,
    selected: selected!,
    iconRects: rects!.split(";").map(parseRect),
  };
}

async function sidebar(client: ControlClient): Promise<Sidebar> {
  const res = await client.request(ControlCommand.TestSidebarThumbnails, []);
  const raw = String(res[1] ?? "");
  const m =
    /hwnd=(\d+) thumbnails=(\d) count=\d+ current=(\d+) rendered=(\d+) ring=\d top=(\S+) bottom=(\S+) rects=(\S*)/.exec(
      raw,
    );
  if (res[0] !== 0 || !m) {
    throw new Error(`sidebar-thumbnails: TestSidebarThumbnails: ${raw}`);
  }
  const rects = new Map<number, Rect>();
  for (const part of m[7]!.split(";").filter(Boolean)) {
    const [page, coords] = part.split(":");
    rects.set(+page!, parseRect(coords!));
  }
  return {
    hwnd: +m[1]!,
    thumbnails: m[2] === "1",
    current: +m[3]!,
    rendered: +m[4]!,
    top: parsePanel(m[5]!),
    bottom: parsePanel(m[6]!),
    rects,
    raw,
  };
}

// what the panels show, e.g. "thumbnails/-": top shows thumbnails, bottom is hidden
function panels(s: Sidebar): string {
  const one = (p: Panel) => (p.visible ? p.view : "-");
  return `${one(s.top)}/${one(s.bottom)}`;
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
  const res = await client.request(ControlCommand.TestPageInfo, []);
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

// clicks a panel's view icon
function clickIcon(p: Panel, view: View) {
  const r = p.iconRects[views.indexOf(view)]!;
  if (r.dx <= 0) {
    throw new Error(`sidebar-thumbnails: the ${view} icon isn't visible`);
  }
  clickPt(p.hwnd, { x: r.x + Math.floor(r.dx / 2), y: r.y + Math.floor(r.dy / 2) });
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

    const want = async (what: string, layout: string) => {
      await waitFor(`${what}: want ${layout}`, async () => panels(await sidebar(client)) === layout).catch(
        async (e) => {
          throw new Error(`${e.message}: ${(await sidebar(client)).raw}`);
        },
      );
    };

    // a command opens its view in the first free panel: the top one, then the
    // bottom one; with both showing, the top one switches to it. The document
    // has bookmarks, so they show on opening
    await want("the bookmarks didn't show on opening", "bookmarks/-");
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await want("Thumbnails didn't show in the bottom panel", "bookmarks/thumbnails");
    let s = await sidebar(client);
    if (s.bottom.enabled !== "111" || s.bottom.selected !== "010") {
      throw new Error(`sidebar-thumbnails: want all view icons, Thumbnails selected: ${s.raw}`);
    }
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await want("Bookmarks didn't hide the top panel", "-/thumbnails");
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await want("Bookmarks didn't show in the top panel", "bookmarks/thumbnails");
    sendCommand(frame, cmdId("CmdFavoriteToggle"));
    await want("Favorites didn't replace Bookmarks on top", "favorites/thumbnails");

    // clicking an icon switches the panel's view; the other panel's view swaps
    clickIcon((await sidebar(client)).top, "bookmarks");
    await want("clicking Bookmarks didn't show Bookmarks", "bookmarks/thumbnails");
    clickIcon((await sidebar(client)).top, "thumbnails");
    await want("clicking Thumbnails on top didn't swap the panels", "thumbnails/bookmarks");
    await waitFor("clicking Thumbnails didn't repaint the sidebar", async () => frameOnScreen(await sidebar(client)));
    s = await sidebar(client);
    if (s.hwnd !== s.top.hwnd || s.top.selected !== "010" || s.bottom.selected !== "100") {
      throw new Error(`sidebar-thumbnails: after the swap: ${s.raw}`);
    }
    clickIcon(s.bottom, "thumbnails");
    await want("clicking Thumbnails in the bottom panel didn't swap back", "bookmarks/thumbnails");
    await waitFor("the thumbnails didn't repaint in the bottom panel", async () =>
      frameOnScreen(await sidebar(client)),
    );
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await want("Bookmarks again didn't hide the top panel", "-/thumbnails");

    // shown again to take the focus
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await want("Thumbnails didn't hide", "-/-");
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await want("Thumbnails didn't show again", "thumbnails/-");
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
    postMessage(hwndPanel, WM_KEYDOWN, VK_DOWN, 0);
    await waitFor("Down didn't go to thumbnail 2", currentIs(2));
    postMessage(hwndPanel, WM_KEYDOWN, VK_UP, 0);
    await waitFor("Up didn't go to thumbnail 1", currentIs(1));
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
    // the clicked page has the blue frame only, no selection border around it
    await waitFor("the click didn't frame page 3", async () => frameOnScreen(await sidebar(client)));
    s = await sidebar(client);
    const r3 = s.rects.get(3)!;
    const row = readWindowDCRow(s.hwnd, r3.x - 12, r3.y + Math.floor(r3.dy / 2), 12);
    if (!row.slice(8, 11).every((c) => c === row[0])) {
      throw new Error(`sidebar-thumbnails: the current page has a selection border: ${row.map((c) => c.toString(16))}`);
    }

    // dragging a page doesn't move it, even in Edit PDF mode
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    s = await sidebar(client);
    const from = pointIn(s, 1, 0.5, 0.5);
    const to = pointIn(s, 3, 0.9, 0.9);
    mouse(s.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, from);
    mouse(s.hwnd, WM_MOUSEMOVE, MK_LBUTTON, to);
    mouse(s.hwnd, WM_LBUTTONUP, 0, to);
    if ((await widths(client)) !== "601,602,603,604") {
      throw new Error("sidebar-thumbnails: dragging a thumbnail moved pages");
    }

    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await want("Thumbnails didn't hide the sidebar", "-/-");
    console.log("sidebar-thumbnails: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
