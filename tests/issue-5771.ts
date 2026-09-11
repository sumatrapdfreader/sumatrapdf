// #5771 / #3389 / #5723: reading bar (highlight or invert mask), per-tab, not on Home.
//
// Run: bun tests/issue-5771.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { postMessage, sleep, VK_ESCAPE, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";

type BarState = {
  on: boolean;
  invert: number;
  home: number;
  autoOn: number;
  yFrac: number;
  height: number;
  bandY: number;
  bandH: number;
  scrollY: number;
};

async function barState(client: ControlClient): Promise<BarState> {
  const res = await client.request(ControlCommand.TestReadingBar, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode !== 0) {
    throw new Error(`issue-5771: TestReadingBar failed: ${out.trim()}`);
  }
  const ok =
    /OK on=(\d+) invert=(\d+) home=(\d+) auto=(\d+) yFrac=(-?\d+) height=(-?\d+) bandY=(-?\d+) bandH=(-?\d+) scrollY=(-?\d+)/.exec(
      out,
    );
  if (!ok) {
    throw new Error(`issue-5771: could not parse: ${out.trim()}`);
  }
  return {
    on: +ok[1]! === 1,
    invert: +ok[2]!,
    home: +ok[3]!,
    autoOn: +ok[4]!,
    yFrac: +ok[5]!,
    height: +ok[6]!,
    bandY: +ok[7]!,
    bandH: +ok[8]!,
    scrollY: +ok[9]!,
  };
}

async function waitBar(
  client: ControlClient,
  pred: (s: BarState) => boolean,
  msg: string,
  ms = 4000,
): Promise<BarState> {
  const deadline = Date.now() + ms;
  let last: BarState | null = null;
  while (Date.now() < deadline) {
    last = await barState(client);
    if (pred(last)) {
      return last;
    }
    await sleep(50);
  }
  throw new Error(`issue-5771: ${msg}: ${JSON.stringify(last)}`);
}

function postKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 1);
  postMessage(hwnd, WM_KEYUP, vk, 1);
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommand(frame, cmdId("CmdToggleReadingBar"));
    let st = await waitBar(client, (s) => s.on && s.bandH > 0 && s.home === 0, "bar did not show");
    const y0 = st.yFrac;
    console.log(`  bar on bandY=${st.bandY} bandH=${st.bandH} yFrac=${st.yFrac} ✓`);

    sendCommand(frame, cmdId("CmdToggleReadingBarInvert"));
    st = await waitBar(client, (s) => s.on && s.invert === 1, "invert did not turn on");
    console.log(`  invert on ✓`);
    sendCommand(frame, cmdId("CmdToggleReadingBarInvert"));
    st = await waitBar(client, (s) => s.on && s.invert === 0, "invert did not turn off");

    sendCommand(frame, cmdId("CmdPrevTab"));
    st = await waitBar(client, (s) => !s.on && s.home === 1, "bar still shown on Home");
    console.log(`  Home tab hid the bar ✓`);

    sendCommand(frame, cmdId("CmdNextTab"));
    st = await waitBar(
      client,
      (s) => s.on && s.home === 0 && s.yFrac === y0,
      "bar did not return with the document tab",
    );
    console.log(`  document tab restored the bar ✓`);

    sendCommand(frame, cmdId("CmdToggleAutomaticallyScroll"));
    const startY = st.scrollY;
    st = await waitBar(
      client,
      (s) => s.on && s.autoOn === 1 && s.scrollY > startY && s.yFrac === y0,
      "bar moved or did not stay while auto-scroll",
      2500,
    );
    console.log(`  auto-scroll moved page (${startY} -> ${st.scrollY}), bar yFrac stayed ${st.yFrac} ✓`);
    sendCommand(frame, cmdId("CmdToggleAutomaticallyScroll"));
    await waitBar(client, (s) => s.on && s.autoOn === 0, "auto-scroll did not stop");

    postKey(frame, VK_ESCAPE);
    st = await waitBar(client, (s) => !s.on, "Esc did not hide the bar");
    console.log(`  Esc hid the bar ✓`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
