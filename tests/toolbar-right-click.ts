// Right or middle click on a toolbar button without a drop-down does nothing;
// only a left click runs the button's command.
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { writeFileSync } from "node:fs";
import {
  packCoords,
  sendMessage,
  sleep,
  MK_LBUTTON,
  MK_MBUTTON,
  MK_RBUTTON,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MBUTTONDOWN,
  WM_MBUTTONUP,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
} from "./winapi.ts";
import { findChildByClass, killAndWait, launchControlled } from "./win-automation.ts";

function makePdf(): string {
  const page = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 3 /Kids [3 0 R 4 0 R 5 0 R] >>",
    page,
    page,
    page,
  ]);
}

async function currentPage(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestCurrentTab, []))[1] ?? "");
  const m = / page=(\d+)/.exec(raw);
  if (!m) throw new Error(`toolbar-right-click: TestCurrentTab: ${raw}`);
  return +m[1]!;
}

async function nextPageButton(client: ControlClient): Promise<{ x: number; y: number }> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const want = cmdId("CmdGoToNextPage");
  const re = /^idx=\d+ cmd=(\d+) hidden=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/gm;
  let m: RegExpExecArray | null;
  while ((m = re.exec(raw)) !== null) {
    if (+m[1]! !== want || m[2] === "1") continue;
    return { x: Math.floor((+m[3]! + +m[5]!) / 2), y: Math.floor((+m[4]! + +m[6]!) / 2) };
  }
  throw new Error(`toolbar-right-click: no Next Page button in:\n${raw}`);
}

async function waitPage(client: ControlClient, want: number): Promise<number> {
  const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const page = await currentPage(client);
    if (page === want || Date.now() > deadline) return page;
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("toolbar-right-click: orig only, skipping");
    return;
  }
  const pdf = tmpPath("toolbar-right-click.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    if (!toolbar) throw new Error("toolbar-right-click: no toolbar");
    const btn = await nextPageButton(client);
    const lp = packCoords(btn.x, btn.y);

    sendMessage(toolbar, WM_RBUTTONDOWN, MK_RBUTTON, lp);
    sendMessage(toolbar, WM_RBUTTONUP, 0, lp);
    sendMessage(toolbar, WM_MBUTTONDOWN, MK_MBUTTON, lp);
    sendMessage(toolbar, WM_MBUTTONUP, 0, lp);
    // a page change would show up within this
    const afterOther = await waitPage(client, 2);
    if (afterOther !== 1) {
      throw new Error(`toolbar-right-click: right / middle click went to page ${afterOther}`);
    }

    sendMessage(toolbar, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    sendMessage(toolbar, WM_LBUTTONUP, 0, lp);
    const afterLeft = await waitPage(client, 2);
    if (afterLeft !== 2) {
      throw new Error(`toolbar-right-click: left click left the page at ${afterLeft}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
