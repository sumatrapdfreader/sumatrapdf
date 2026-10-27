// issue #6405: switching between an image tab and an html tab scrolled the
// image tab to the top after a few switches
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { ensureModifierKeysUp, findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { clientToScreen, getClientRect, getScrollInfo, packCoords, sendMessage, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WM_COMMAND = 0x0111;
const WHEEL_DELTA = 120;
const SWITCHES = 30;

async function wheelDown(canvas: number, notches: number): Promise<void> {
  await ensureModifierKeysUp();
  const cr = getClientRect(canvas);
  const mid = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
  const lp = packCoords(mid.x, mid.y);
  const wp = (-WHEEL_DELTA << 16) >>> 0;
  for (let i = 0; i < notches; i++) {
    sendMessage(canvas, WM_MOUSEWHEEL, wp, lp);
  }
}

async function currentTab(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return String(res[1] ?? "").trim();
}

// the window also has a Home tab, so cycle until the wanted tab is current
async function selectTab(client: ControlClient, frame: number, name: string): Promise<void> {
  let last = await currentTab(client);
  for (let i = 0; i < 4 && !last.includes(name); i++) {
    sendMessage(frame, WM_COMMAND, cmdId("CmdNextTab"), 0);
    last = await currentTab(client);
  }
  if (!last.includes(name)) {
    throw new Error(`issue-6405: tab ${name} not selected: ${last}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6405");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const jpg = join(dir, "img.jpg");
  copyFileSync(join(import.meta.dir, "issue-6229.jpg"), jpg);
  const html = join(dir, "page.html");
  writeFileSync(html, "<!DOCTYPE html><html><body><p>html tab</p></body></html>\n");

  const { proc, client, frame } = await launchControlled([jpg, html]);
  try {
    await client.waitForRenderIdle();
    await selectTab(client, frame, "img.jpg");
    sendMessage(frame, WM_COMMAND, cmdId("CmdZoomFitWidth"), 0);
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6405: no canvas");
    }
    await wheelDown(canvas, 40);
    await sleep(300);
    const want = getScrollInfo(canvas).pos;
    if (want <= 0) {
      throw new Error(`issue-6405: image did not scroll: ${JSON.stringify(getScrollInfo(canvas))}`);
    }

    // the bug depends on timing: vary how long each tab stays active
    for (let i = 0; i < SWITCHES; i++) {
      await selectTab(client, frame, "page.html");
      await sleep(50 + ((i * 97) % 400));
      await selectTab(client, frame, "img.jpg");
      await sleep(100);
      const pos = getScrollInfo(canvas).pos;
      if (pos !== want) {
        throw new Error(`issue-6405: switch ${i + 1}: image scroll ${want} -> ${pos}`);
      }
    }
    console.log(`issue-6405: image scroll stayed ${want} over ${SWITCHES} switches`);
    await client.quit();
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
