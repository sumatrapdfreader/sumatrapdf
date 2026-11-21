// Dragging a tab and dropping it on another window moves it there, appended
// after that window's tabs (orig's MainWindowTabMigration).
//
// Run: bun tests/tab-drag-to-window.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  enumWindows,
  getClassName,
  getClientRect,
  getDpiForWindow,
  getWindowPid,
  getWorkArea,
  HWND_TOPMOST,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  setProcessDpiAware,
  setWindowPos,
  SWP_NOACTIVATE,
  topLevelWindowFromPoint,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findChildByClass, killAndWait, launchControlled } from "./win-automation.ts";

const TABS_CLASS = "SumatraTabsCtrlClass";

type TabRow = { win: number; tab: number; ctrl: boolean; path: string };

// tabIdx < 0 only lists every window's tabs
async function migrate(client: ControlClient, srcWin: number, tabIdx: number, dstWin: number): Promise<TabRow[]> {
  const res = await client.request(ControlCommand.TestMigrateTab, [srcWin, tabIdx, dstWin]);
  const out = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`tab-drag-to-window: TestMigrateTab failed: ${out}`);
  }
  return [...out.matchAll(/^win=(\d+) tab=(\d+) ctrl=(\d) path=(.*)$/gm)].map((m) => ({
    win: +m[1]!,
    tab: +m[2]!,
    ctrl: m[3] === "1",
    path: m[4]!.trim(),
  }));
}

// "win0: a.pdf b.pdf | win1: c.pdf", loaded documents only
function describe(rows: TabRow[]): string {
  const wins: string[][] = [];
  for (const r of rows) {
    if (!r.ctrl) {
      continue;
    }
    (wins[r.win] ??= []).push(basename(r.path));
  }
  return [...wins].map((w, i) => `win${i}: ${(w ?? []).join(" ")}`).join(" | ");
}

async function waitFor(client: ControlClient, want: string, what: string): Promise<void> {
  await pollUntil(
    async () => describe(await migrate(client, 0, -1, 0)),
    (got) => got === want,
    {
      timeoutMs: 10000 * SLOW_BUILD_FACTOR,
      error: (got) => `tab-drag-to-window: ${what}: want '${want}', got '${got}'`,
    },
  );
}

function otherFrame(pid: number, frame: number): number {
  const cls = getClassName(frame);
  let found = 0;
  enumWindows((h) => {
    if (h !== frame && getWindowPid(h) === pid && isWindowVisible(h) && getClassName(h) === cls) {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

// where to press on the tab named `name`: the window that gets the mouse
// messages and the point in its client pixels
async function tabPoint(
  client: ControlClient,
  frame: number,
  name: string,
): Promise<{ hwnd: number; x: number; y: number }> {
  const raw = String((await client.request(ControlCommand.TestTabWidths, []))[1] ?? "");
  if (USE_NG) {
    // dips in frame coordinates
    const scale = getDpiForWindow(frame) / 96;
    for (const m of raw.matchAll(/^dx=(\d+) text=(.*)\n {2}at x=(-?\d+) y=(-?\d+) dy=(\d+)$/gm)) {
      if (m[2]!.trim() === name) {
        const x = +m[3]! + Math.floor(+m[1]! / 3);
        const y = +m[4]! + Math.floor(+m[5]! / 2);
        return { hwnd: frame, x: Math.round(x * scale), y: Math.round(y * scale) };
      }
    }
    throw new Error(`tab-drag-to-window: no tab '${name}':\n${raw}`);
  }
  // orig: the tab bar's pixels, tabs side by side from its left
  const tabs = findChildByClass(frame, TABS_CLASS);
  if (!tabs) {
    throw new Error("tab-drag-to-window: no tab bar");
  }
  let x = 0;
  for (const m of raw.matchAll(/^dx=(\d+) text=(.*)$/gm)) {
    if (m[2]!.trim() === name) {
      return { hwnd: tabs, x: x + Math.floor(+m[1]! / 3), y: Math.floor(getClientRect(tabs).bottom / 2) };
    }
    x += +m[1]!;
  }
  throw new Error(`tab-drag-to-window: no tab '${name}':\n${raw}`);
}

export async function testit(): Promise<void> {
  // client pixels and screen pixels are the same thing only when DPI aware
  setProcessDpiAware();
  const dir = tmpPath("tab-drag-to-window");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfs = ["a", "b", "c"].map((n) => join(dir, `${n}.pdf`));
  for (const p of pdfs) {
    writeFileSync(p, makePdf(2, 601, 0, 0), "latin1");
  }

  const { proc, client, frame } = await launchControlled(pdfs);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await waitFor(client, "win0: a.pdf b.pdf c.pdf", "opened");

    // c.pdf into a second window
    const rows = await migrate(client, 0, -1, 0);
    const c = rows.find((r) => basename(r.path) === "c.pdf")!;
    await migrate(client, 0, c.tab, -1);
    await waitFor(client, "win0: a.pdf b.pdf | win1: c.pdf", "c.pdf in a new window");
    const frame2 = await pollUntil(
      async () => otherFrame(proc.pid!, frame),
      (h) => h !== 0,
      { timeoutMs: 5000 * SLOW_BUILD_FACTOR, error: () => "tab-drag-to-window: no second window" },
    );

    // side by side, above whatever else is on screen: the drop goes to the
    // window really there
    const wa = getWorkArea();
    const half = Math.floor((wa.right - wa.left) / 2);
    const dy = Math.min(600, wa.bottom - wa.top);
    setWindowPos(frame, wa.left, wa.top, half, dy, SWP_NOACTIVATE, HWND_TOPMOST);
    setWindowPos(frame2, wa.left + half, wa.top, half, dy, SWP_NOACTIVATE, HWND_TOPMOST);
    await client.waitForRenderIdle();

    const from = await tabPoint(client, frame, "a.pdf");
    // the drop: over the second window's tab strip, as high in it as the tab
    const fromScreen = clientToScreen(from.hwnd, from.x, from.y);
    const frameOrigin = clientToScreen(frame, 0, 0);
    const frame2Origin = clientToScreen(frame2, 0, 0);
    const drop = {
      x: frame2Origin.x + Math.floor(getClientRect(frame2).right * 0.6),
      y: frame2Origin.y + (fromScreen.y - frameOrigin.y),
    };
    const hit = topLevelWindowFromPoint(drop.x, drop.y);
    if (hit !== frame2) {
      throw new Error(`tab-drag-to-window: the drop point is covered by window 0x${hit.toString(16)}`);
    }
    // the release arrives at the window the press started in, in its coordinates
    const origin = clientToScreen(from.hwnd, 0, 0);
    const up = { x: drop.x - origin.x, y: drop.y - origin.y };
    postMessage(from.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(from.x, from.y));
    postMessage(from.hwnd, WM_MOUSEMOVE, MK_LBUTTON, packCoords(from.x, from.y + 40));
    postMessage(from.hwnd, WM_MOUSEMOVE, MK_LBUTTON, packCoords(up.x, up.y));
    postMessage(from.hwnd, WM_LBUTTONUP, 0, packCoords(up.x, up.y));

    await waitFor(client, "win0: b.pdf | win1: c.pdf a.pdf", "a.pdf dropped on the second window");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("tab-drag-to-window: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
