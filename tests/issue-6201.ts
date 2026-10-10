// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6201
//
// ScrollEdgeTurnsPage = false keeps the wheel on the current page: scrolling a
// zoomed-in page in single page view stops at its bottom instead of going to
// the next page. Default (true) is the old behaviour, which tests/issue-5069.ts
// covers.
//
// Run:  bun tests/issue-6201.ts [--no-build]

import { writeFileSync, mkdirSync, rmSync } from "node:fs";
import { runStandalone, tmpPath, makeTopBottomPdf } from "./util.ts";
import { ControlClient, ControlCommand } from "./control.ts";
import { launchControlled, findCanvas, killAndWait, ensureModifierKeysUp } from "./win-automation.ts";
import { sendMessage, getScrollPos, sleep, SB_VERT } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DOWN = 0xff880000n;
const WHEEL_UP = 0x00780000n;

const PAGE_COUNT = 3;

async function currentPage(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    const m = /OK page=(\d+)/.exec(String(res[1] ?? ""));
    if (m) {
      return +m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-6201: could not read the current page");
    }
    await sleep(100);
  }
}

async function wheel(canvas: number, wp: bigint): Promise<void> {
  await ensureModifierKeysUp();
  sendMessage(canvas, WM_MOUSEWHEEL, wp, 0n);
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6201.pdf");
  writeFileSync(pdfPath, makeTopBottomPdf(PAGE_COUNT));
  const appdata = tmpPath("issue-6201-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    `${appdata}/SumatraPDF-settings.txt`,
    ["RestoreSession = false", "SmoothScroll = false", "ScrollEdgeTurnsPage = false", ""].join("\n"),
  );

  // 200%: the page is taller than the window, so there is something to scroll
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "200",
    pdfPath,
  ]);
  const canvas = findCanvas(frame);
  try {
    await client.waitForRenderIdle();

    // wheel down until the page stops moving: it must stay on page 1
    let y = getScrollPos(canvas, SB_VERT);
    let bottom = y;
    for (let i = 0; i < 40; i++) {
      await wheel(canvas, WHEEL_DOWN);
      const page = await currentPage(client);
      if (page !== 1) {
        throw new Error(`issue-6201: wheel left page 1 for page ${page} with ScrollEdgeTurnsPage = false`);
      }
      const next = getScrollPos(canvas, SB_VERT);
      if (next === y) {
        bottom = next;
        break;
      }
      y = next;
      bottom = next;
    }
    if (bottom <= 0) {
      throw new Error(`issue-6201: expected the zoomed page to scroll, scrollY stayed ${bottom}`);
    }

    // and back up: still page 1, back at the top
    for (let i = 0; i < 40; i++) {
      await wheel(canvas, WHEEL_UP);
      const page = await currentPage(client);
      if (page !== 1) {
        throw new Error(`issue-6201: wheeling up left page 1 for page ${page}`);
      }
      if (getScrollPos(canvas, SB_VERT) === 0) {
        break;
      }
    }
    if (getScrollPos(canvas, SB_VERT) !== 0) {
      throw new Error("issue-6201: wheeling up did not reach the top of page 1");
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
