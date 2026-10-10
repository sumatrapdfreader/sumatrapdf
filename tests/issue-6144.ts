// issue #6144: in a comic (single page, page taller than the window), wheeling
// at the bottom before the next page has rendered queued many GoToNextPage
// calls and skipped 5–10 pages. A burst of wheel-down at the bottom of page 1
// must land on page 2, not far ahead.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath, crc32, makePng } from "./util.ts";
import { findCanvas, launchControlled, killAndWait, ensureModifierKeysUp } from "./win-automation.ts";
import { clientToScreen, getClientRect, packCoords, sendMessage, setCursorPos, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA = 120;
const PAGE_COUNT = 12;

function makeZip(entries: { name: string; data: Buffer }[]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "latin1");
    const crc = crc32(e.data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0);
    lh.writeUInt16LE(20, 4);
    lh.writeUInt32LE(crc, 14);
    lh.writeUInt32LE(e.data.length, 18);
    lh.writeUInt32LE(e.data.length, 22);
    lh.writeUInt16LE(name.length, 26);
    const local = Buffer.concat([lh, name, e.data]);
    locals.push(local);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0);
    ch.writeUInt16LE(20, 4);
    ch.writeUInt16LE(20, 6);
    ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(e.data.length, 20);
    ch.writeUInt32LE(e.data.length, 24);
    ch.writeUInt16LE(name.length, 28);
    ch.writeUInt32LE(offset, 42);
    centrals.push(Buffer.concat([ch, name]));
    offset += local.length;
  }
  const central = Buffer.concat(centrals);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(central.length, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, central, end]);
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
      throw new Error(`issue-6144: could not read current page: ${String(res[1] ?? "").trim()}`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6144");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pages = Array.from({ length: PAGE_COUNT }, (_, i) => ({
    name: `${String(i + 1).padStart(3, "0")}.png`,
    data: makePng(400, 900, [40 + i * 10, 40, 200 - i * 8]),
  }));
  const cbz = join(dir, "issue-6144.cbz");
  writeFileSync(cbz, makeZip(pages));

  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "ShowStartPage = false", "SmoothScroll = true", "MouseWheelTurnsPage = false", ""].join(
      "\n",
    ),
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    cbz,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6144: no canvas");
    }
    if ((await currentPage(client)) !== 1) {
      throw new Error("issue-6144: expected to start on page 1");
    }

    const cr = getClientRect(canvas);
    const mid = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    setCursorPos(mid.x, mid.y);
    const lp = packCoords(mid.x, mid.y);
    const wp = (-WHEEL_DELTA << 16) >>> 0;
    await ensureModifierKeysUp();
    for (let i = 0; i < 10; i++) {
      sendMessage(canvas, WM_MOUSEWHEEL, wp, lp);
    }
    await client.waitForRenderIdle();
    const page = await currentPage(client);
    if (page > 2) {
      throw new Error(`issue-6144: wheel burst skipped to page ${page}, want page 2`);
    }
    if (page < 2) {
      throw new Error(`issue-6144: wheel at page bottom did not turn the page (still ${page})`);
    }
    console.log(`issue-6144: wheel burst stayed on page ${page}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6144: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
