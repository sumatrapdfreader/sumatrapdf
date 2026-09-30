// The wheel over the sidebar's thumbnails scrolls them, never the document: not
// a touchpad's small deltas, not a horizontal wheel, not past the list's end.
//
// Run: bun tests/sidebar-thumbnails-wheel.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import { clientToScreen, getScrollPos, packCoords, sendMessage, SB_HORZ, SB_VERT, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const WM_MOUSEWHEEL = 0x020a;
const WM_MOUSEHWHEEL = 0x020e;
const WHEEL_DELTA = 120;
// a precision touchpad sends many small deltas instead of 120 per notch
const TOUCHPAD_DELTA = 30;

type Thumbs = { hwnd: number; shown: boolean; page1Y: number; raw: string };

async function thumbs(client: ControlClient): Promise<Thumbs> {
  const res = await client.request(ControlCommand.TestSidebarThumbnails, []);
  const raw = String(res[1] ?? "");
  const m = /hwnd=(\d+) thumbnails=(\d).* rects=1:-?\d+,(-?\d+),/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`sidebar-thumbnails-wheel: TestSidebarThumbnails: ${raw}`);
  }
  return { hwnd: +m[1]!, shown: m[2] === "1", page1Y: +m[3]!, raw };
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`sidebar-thumbnails-wheel: ${what}`);
    }
    await sleep(50);
  }
}

// a wheel message as Windows sends it: delta in the high word, cursor in screen coords
function wheel(hwnd: number, msg: number, delta: number) {
  const pt = clientToScreen(hwnd, 40, 60);
  const wp = BigInt((delta & 0xffff) << 16);
  sendMessage(hwnd, msg, wp, packCoords(pt.x, pt.y));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("sidebar-thumbnails-wheel");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(12, 601, 3, 0), "latin1");

  // zoomed in, so the document can scroll both ways
  const { proc, client, frame } = await launchControlled(["-zoom", "400", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await waitFor("Thumbnails didn't show", async () => (await thumbs(client)).shown);
    const canvas = findCanvas(frame);
    const x0 = getScrollPos(canvas, SB_HORZ);
    const y0 = getScrollPos(canvas, SB_VERT);
    const docMoved = (what: string) => {
      const x = getScrollPos(canvas, SB_HORZ);
      const y = getScrollPos(canvas, SB_VERT);
      if (x !== x0 || y !== y0) {
        throw new Error(`sidebar-thumbnails-wheel: ${what} scrolled the document: ${x0},${y0} -> ${x},${y}`);
      }
    };

    let t = await thumbs(client);
    for (let i = 0; i < 4; i++) {
      wheel(t.hwnd, WM_MOUSEWHEEL, -TOUCHPAD_DELTA);
    }
    await sleep(200);
    docMoved("a touchpad scroll");
    const after = await thumbs(client);
    if (after.page1Y >= t.page1Y) {
      throw new Error(`sidebar-thumbnails-wheel: a touchpad scroll didn't scroll the thumbnails: ${after.raw}`);
    }

    wheel(t.hwnd, WM_MOUSEHWHEEL, WHEEL_DELTA);
    await sleep(200);
    docMoved("a horizontal wheel");

    // at the end of the list, more wheel stays in it
    for (let i = 0; i < 30; i++) {
      wheel(t.hwnd, WM_MOUSEWHEEL, -WHEEL_DELTA);
    }
    await sleep(200);
    docMoved("the wheel past the end of the thumbnails");
    console.log("sidebar-thumbnails-wheel: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
