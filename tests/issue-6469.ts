// #6469, four of its items:
// 2: the Rotate buttons' tooltips showed the menu's "&" ("Rotate &Left")
// 4: Close Other Tabs (or Left / Right) from a command left the tab bar blank:
//    the strip rebuilt its tabs and was never repainted
// 5: Go To Home Page and New Tab were missing from the command palette
// 6: the thumbnails sidebar drew only whole rows, leaving the strip below empty
// 7: a click in the thumbnails sidebar drew the keyboard's dotted focus ring
//
// Run: bun tests/issue-6469.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findChildByClass, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  clientToScreen,
  getClientRect,
  MK_LBUTTON,
  packCoords,
  postMessage,
  readWindowDCRow,
  sendMessage,
  setCursorPos,
  VK_DOWN,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
} from "./winapi.ts";

const TABS_CLASS = "SumatraTabsCtrlClass";
// a blank strip is one color; tabs add their labels and backgrounds
const MIN_TAB_COLORS = 3;
// the focus ring is a dashed line one pixel inside the list's bottom edge
const RING_INSET_DY = 2;
const MIN_RING_PIXELS = 20;
const MAX_NO_RING_PIXELS = 2;

// dark pixels where the thumbnails list draws the bottom of its focus ring
function ringPixels(hwnd: number): number {
  const rc = getClientRect(hwnd);
  const row = readWindowDCRow(hwnd, 0, rc.bottom - rc.top - RING_INSET_DY, rc.right - rc.left);
  const isDark = (c: number) => (c & 0xff) < 128 && ((c >> 8) & 0xff) < 128 && ((c >> 16) & 0xff) < 128;
  return row.filter(isDark).length;
}

// the ring follows the keyboard: shown when the panel takes the focus from a
// command and on Up / Down, not after a click
async function checkThumbnailsFocusRing(client: ControlClient, frame: number): Promise<void> {
  sendCommand(frame, cmdId("CmdToggleThumbnails"));
  const state = async () => String((await client.request(ControlCommand.TestSidebarThumbnails, []))[1] ?? "");
  const shown = await pollUntil(state, (s) => /thumbnails=1 .*rects=1:\d+,\d+,[1-9]/.test(s), {
    error: (s) => `issue-6469: the thumbnails didn't show: ${s}`,
  });
  const hwnd = +/hwnd=(\d+)/.exec(shown)![1]!;
  await pollUntil(
    () => ringPixels(hwnd),
    (n) => n >= MIN_RING_PIXELS,
    { error: (n) => `issue-6469: no focus ring after showing the thumbnails (${n} pixels)` },
  );

  const r = /rects=1:(\d+),(\d+),\d+,\d+/.exec(shown)!;
  const pt = { x: +r[1]! + 20, y: +r[2]! + 20 };
  // SetCapture injects a WM_MOUSEMOVE at the real cursor: keep it where we click
  const screen = clientToScreen(hwnd, pt.x, pt.y);
  setCursorPos(screen.x, screen.y);
  sendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(pt.x, pt.y));
  sendMessage(hwnd, WM_LBUTTONUP, 0, packCoords(pt.x, pt.y));
  await pollUntil(
    () => ringPixels(hwnd),
    (n) => n <= MAX_NO_RING_PIXELS,
    { error: (n) => `issue-6469: a click left the focus ring on (${n} pixels)` },
  );

  postMessage(hwnd, WM_KEYDOWN, VK_DOWN, 0);
  await pollUntil(
    () => ringPixels(hwnd),
    (n) => n >= MIN_RING_PIXELS,
    { error: (n) => `issue-6469: Down didn't bring the focus ring back (${n} pixels)` },
  );
}

async function checkTooltips(client: ControlClient): Promise<void> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const withAmp = raw.split("\n").filter((l) => /^idx=.* text=.*&/.test(l));
  if (withAmp.length > 0) {
    throw new Error(`issue-6469: tooltips with '&':\n${withAmp.join("\n")}`);
  }
}

async function checkPalette(client: ControlClient): Promise<void> {
  for (const cmd of ["CmdGoToHomePage", "CmdNewTab"]) {
    const raw = String((await client.request(ControlCommand.TestCommandVisibility, [cmd, "palette"]))[1] ?? "");
    if (!/vis=show/.test(raw)) {
      throw new Error(`issue-6469: ${cmd} isn't in the command palette: ${raw.trim()}`);
    }
  }
}

// the strip as the screen has it: PrintWindow would paint it afresh and hide the bug
function tabColors(tabs: number): number {
  const rc = getClientRect(tabs);
  const row = readWindowDCRow(tabs, 0, Math.floor((rc.bottom - rc.top) / 2), rc.right - rc.left);
  return new Set(row).size;
}

// enough pages that the thumbnails don't fit; widths 600.. keep them apart
const MANY_PAGES = 40;

// a row cut by the sidebar's bottom edge is drawn, so its thumbnails have a rect
async function checkPartialRow(): Promise<void> {
  const pdf = tmpPath("issue-6469-many.pdf");
  writeFileSync(pdf, makePdf(MANY_PAGES, 600, 0, 0), "latin1");
  const { proc, client, frame } = await launchControlled(["-view", "single page", pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    const state = async () => String((await client.request(ControlCommand.TestSidebarThumbnails, []))[1] ?? "");
    const shown = await pollUntil(state, (s) => /thumbnails=1 .*rects=1:\d+,\d+,[1-9]/.test(s), {
      error: (s) => `issue-6469: the thumbnails didn't show: ${s}`,
    });
    const hwnd = +/hwnd=(\d+)/.exec(shown)![1]!;
    const rc = getClientRect(hwnd);
    const panelDy = rc.bottom - rc.top;
    const rects = [...shown.matchAll(/(\d+):(-?\d+),(-?\d+),(\d+),(\d+);/g)].map((m) => ({
      y: +m[3]!,
      dy: +m[5]!,
    }));
    const cut = rects.some((r) => r.dy > 0 && r.y < panelDy && r.y + r.dy > panelDy);
    // a row can end exactly at the edge; only then is nothing cut
    const fits = rects.some((r) => r.dy > 0 && r.y + r.dy === panelDy);
    if (!cut && !fits) {
      throw new Error(`issue-6469: no thumbnail row is cut by the bottom edge (panel ${panelDy}): ${shown}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP issue-6469: orig's toolbar and tab strip");
    return;
  }
  const files = ["a", "b", "c"].map((n) => {
    const p = tmpPath(`issue-6469-${n}.pdf`);
    writeFileSync(p, makePdf(2, 600, 0, 0), "latin1");
    return p;
  });
  const { proc, client, frame } = await launchControlled(files);
  try {
    await client.waitForRenderIdle();
    await checkTooltips(client);
    await checkPalette(client);

    const tabs = findChildByClass(frame, TABS_CLASS);
    if (!tabs) {
      throw new Error("issue-6469: no tab strip");
    }
    await pollUntil(
      () => tabColors(tabs),
      (n) => n >= MIN_TAB_COLORS,
      { error: (n) => `issue-6469: the tab strip isn't drawn before closing (${n} colors)` },
    );
    sendCommand(frame, cmdId("CmdCloseOtherTabs"));
    await pollUntil(
      async () => (await client.request(ControlCommand.TestCurrentTab, []))[1],
      () => true,
    );
    await pollUntil(
      () => tabColors(tabs),
      (n) => n >= MIN_TAB_COLORS,
      { error: (n) => `issue-6469: Close Other Tabs left the tab strip blank (${n} colors)` },
    );
    await checkThumbnailsFocusRing(client, frame);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  await checkPartialRow();
  console.log("issue-6469: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
