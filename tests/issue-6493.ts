// Test for issue #6493: InvertMouseWheel makes the wheel scroll, and turn
// pages, the other way.
//
// Posts a wheel-down notch to a zoomed-in document and checks which way the
// vertical scrollbar moves with the setting off and on, then that it turns to
// the previous page with MouseWheelTurnsPage.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { makeTopBottomPdf, runStandalone, tmpPath } from "./util.ts";
import { ensureModifierKeysUp, findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { getScrollPos, SB_VERT, sendMessage, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DOWN = BigInt((-120 << 16) >>> 0);
const kPageCount = 3;
const kWaitMs = 3000;

async function wheelDown(canvas: number): Promise<void> {
  await ensureModifierKeysUp();
  sendMessage(canvas, WM_MOUSEWHEEL, WHEEL_DOWN, 0n);
}

async function currentPage(client: ControlClient): Promise<number> {
  const [, raw] = await client.request(ControlCommand.TestScrollState, []);
  const m = /page=(\d+)/.exec(String(raw ?? ""));
  return m ? Number(m[1]) : 0;
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

async function withSession(
  name: string,
  settings: string[],
  fn: (client: ControlClient, canvas: number) => Promise<void>,
): Promise<void> {
  const dir = tmpPath(`issue-6493-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-6493.pdf");
  writeFileSync(pdf, makeTopBottomPdf(kPageCount));
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "SmoothScroll = false", ...settings, ""].join("\n"),
  );
  const { proc, client, frame } = await launchControlled(["-appdata", dir, "-zoom", "200", pdf]);
  try {
    await client.waitForRenderIdle();
    await fn(client, findCanvas(frame));
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// moves down first so there is room to scroll either way, then one wheel-down notch
async function wheelDirection(client: ControlClient, canvas: number): Promise<"down" | "up"> {
  await invoke(client, "CmdScrollDown 10");
  let before = 0;
  for (let i = 0; i < 50 && before === 0; i++) {
    await sleep(50);
    before = getScrollPos(canvas, SB_VERT);
  }
  await wheelDown(canvas);
  const deadline = Date.now() + kWaitMs;
  while (Date.now() < deadline) {
    const pos = getScrollPos(canvas, SB_VERT);
    if (pos !== before) {
      return pos > before ? "down" : "up";
    }
    await sleep(50);
  }
  throw new Error(`issue-6493: a wheel notch didn't scroll (pos=${before})`);
}

export async function testit(): Promise<void> {
  await withSession("off", [], async (client, canvas) => {
    const dir = await wheelDirection(client, canvas);
    if (dir !== "down") {
      throw new Error(`issue-6493: wheel down scrolled ${dir} without InvertMouseWheel`);
    }
  });

  await withSession("on", ["InvertMouseWheel = true"], async (client, canvas) => {
    const dir = await wheelDirection(client, canvas);
    if (dir !== "up") {
      throw new Error(`issue-6493: wheel down scrolled ${dir} with InvertMouseWheel`);
    }
  });

  await withSession("turns-page", ["InvertMouseWheel = true", "MouseWheelTurnsPage = true"], async (client, canvas) => {
    await invoke(client, "CmdGoToNextPage");
    for (let i = 0; i < 50 && (await currentPage(client)) !== 2; i++) {
      await sleep(50);
    }
    await wheelDown(canvas);
    let page = 0;
    const deadline = Date.now() + kWaitMs;
    while (Date.now() < deadline) {
      page = await currentPage(client);
      if (page !== 2) {
        break;
      }
      await sleep(50);
    }
    if (page !== 1) {
      throw new Error(`issue-6493: wheel down went from page 2 to ${page} with InvertMouseWheel, want 1`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
