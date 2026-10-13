// issue #3755: in single page / fit page a wheel notch (120) turns a page. A
// precision touchpad reports finger travel in fractions of a notch and a
// swipe adds up to many notches, so one swipe raced through a dozen pages.
// Fractional deltas must need a window's worth of travel per page, while a
// real notch still turns exactly one.
//
// Run: bun tests/issue-3755.ts [--no-build]
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { ensureModifierKeysUp, findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { clientToScreen, getClientRect, packCoords, sendMessage, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA = 120;
const PAGE_COUNT = 40;
// a touchpad event: a fifth of a notch
const TOUCHPAD_DELTA = 24;
// ten notches of touchpad travel: the old code turned ten pages
const TOUCHPAD_EVENTS = (10 * WHEEL_DELTA) / TOUCHPAD_DELTA;

function makePdf(): string {
  const objs = ["<< /Type /Catalog /Pages 2 0 R >>"];
  const kids: string[] = [];
  for (let i = 0; i < PAGE_COUNT; i++) {
    kids.push(`${3 + i} 0 R`);
  }
  objs.push(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${PAGE_COUNT} >>`);
  for (let i = 0; i < PAGE_COUNT; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  }
  return assemblePdf(objs);
}

async function currentPage(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
    const m = /OK page=(\d+)/.exec(String(res[1] ?? ""));
    if (m) {
      return +m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-3755: could not read current page: ${String(res[1] ?? "").trim()}`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-3755");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "ShowStartPage = false", "MouseWheelTurnsPage = false", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-3755: no canvas");
    }
    const cr = getClientRect(canvas);
    const mid = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    const lp = packCoords(mid.x, mid.y);
    const wheel = (delta: number) => sendMessage(canvas, WM_MOUSEWHEEL, ((delta << 16) >>> 0) & 0xffff0000, lp);
    await ensureModifierKeysUp();

    // a touchpad swipe worth ten notches turns at most a page or two
    for (let i = 0; i < TOUCHPAD_EVENTS; i++) {
      wheel(-TOUCHPAD_DELTA);
    }
    await client.waitForRenderIdle();
    const afterSwipe = await currentPage(client);
    if (afterSwipe > 3) {
      throw new Error(`issue-3755: a touchpad swipe raced to page ${afterSwipe}, want at most 3`);
    }

    // a long swipe still gets somewhere
    for (let i = 0; i < TOUCHPAD_EVENTS * 8; i++) {
      wheel(-TOUCHPAD_DELTA);
    }
    await client.waitForRenderIdle();
    const afterLongSwipe = await currentPage(client);
    if (afterLongSwipe <= afterSwipe) {
      throw new Error(`issue-3755: a long touchpad swipe did not turn the page (still ${afterLongSwipe})`);
    }

    // a wheel notch turns exactly one page. A modifier stuck down meanwhile
    // (RDP does that here) would make it a zoom or a horizontal scroll
    await sleep(1200);
    await ensureModifierKeysUp();
    wheel(-WHEEL_DELTA);
    await client.waitForRenderIdle();
    const afterNotch = await currentPage(client);
    if (afterNotch !== afterLongSwipe + 1) {
      throw new Error(`issue-3755: a wheel notch went from page ${afterLongSwipe} to ${afterNotch}, want +1`);
    }
    console.log(`issue-3755: swipe ${afterSwipe}, long swipe ${afterLongSwipe}, notch ${afterNotch}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
