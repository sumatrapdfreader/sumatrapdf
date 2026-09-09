// Crash 2026-09-09-15-43-0ee0: the home page layout was cached in one global,
// but every window has its own home chrome laid out and painted from it.
// Opening a second window rebuilt the cache for that window, and the first one
// then painted with the second window's geometry (and pointers into the
// rebuilt, freed thumbs vector).
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowRect,
  moveWindow,
  type Rect,
  repaintWindow,
  setCursorPos,
  sleep,
} from "./winapi";
import { CANVAS_CLASS, findCanvas, FRAME_CLASS, killAndWait, launchControlled, sendCommand } from "./win-automation";
import type { ControlClient } from "./control.ts";

const nFiles = 6;

function makeAppDir(): string {
  const dir = tmpPath("home-two-windows");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${String(i).padStart(2, "0")}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

function frames(pid: number): number[] {
  const out: number[] = [];
  enumWindows((h) => {
    if (getWindowPid(h) === pid && getClassName(h) === FRAME_CLASS) {
      out.push(h);
    }
    return true;
  });
  return out;
}

const inside = (r: Rect, o: Rect) => r.left >= o.left && r.top >= o.top && r.right <= o.right && r.bottom <= o.bottom;

// the home search box: the only Edit the home page places over the canvas. Its
// width comes from that window's own layout, so it says the window laid out.
function homeSearchDx(frame: number): number {
  const canvas = findCanvas(frame);
  if (!canvas) {
    return 0;
  }
  const rcCanvas = getWindowRect(canvas);
  let dx = 0;
  enumChildWindows(frame, (h) => {
    if (getClassName(h) !== "Edit") {
      return true;
    }
    const r = getWindowRect(h);
    if (!inside(r, rcCanvas)) {
      return true;
    }
    dx = r.right - r.left;
    return false;
  });
  return dx;
}

async function searchRect(client: ControlClient, timeoutMs = 8000): Promise<number[]> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const h = await client.homeSelection();
    if (h.ready) {
      return h.search;
    }
    if (Date.now() > deadline) {
      throw new Error(`home-two-windows: home page never laid out (${h.raw})`);
    }
    await sleep(50);
  }
}

const same = (a: number[], b: number[]) => a.length === b.length && a.every((v, i) => v === b[i]);

export async function testit(): Promise<void> {
  // a cursor over the thumbnails keeps changing the hover selection
  setCursorPos(0, 0);
  const dir = makeAppDir();
  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    // the control commands report the first window, so this is its layout
    const first = await searchRect(client);
    const firstDx = homeSearchDx(frame);

    // second window: its home page lays out at a clearly different width
    sendCommand(frame, cmdId("CmdNewWindow"));
    const deadline = Date.now() + 8000;
    let second = 0;
    while (!second && Date.now() < deadline) {
      second = frames(proc.pid!).find((h) => h !== frame) ?? 0;
      await sleep(100);
    }
    if (!second) {
      throw new Error("home-two-windows: second window never appeared");
    }
    moveWindow(second, 40, 40, 700, 620);
    repaintWindow(second);
    let secondDx = 0;
    while (Date.now() < deadline) {
      secondDx = homeSearchDx(second);
      if (secondDx > 0 && secondDx !== firstDx) {
        break;
      }
      await sleep(100);
    }
    if (!secondDx || secondDx === firstDx) {
      throw new Error(`home-two-windows: second window did not lay out its home page (search dx ${secondDx})`);
    }

    // the first window keeps its own layout: repainting it must not adopt the
    // second window's
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error(`home-two-windows: no ${CANVAS_CLASS} in the first window`);
    }
    repaintWindow(canvas);
    const back = await searchRect(client);
    if (!same(back, first)) {
      throw new Error(
        `home-two-windows: first window painted with the second window's layout: search=${back}, expected ${first}`,
      );
    }
    if (homeSearchDx(frame) !== firstDx) {
      throw new Error(`home-two-windows: first window's search box was moved by the second window's layout`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
