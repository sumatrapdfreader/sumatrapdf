// #6169 / #1680: Acrobat-style Automatically Scroll (hands-free pan + bottom bar).
//
// Run: bun tests/issue-6169.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { postMessage, sleep, VK_DOWN, VK_ESCAPE, VK_SPACE, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";

type BarState = {
  visible: boolean;
  paused: number;
  dir: number;
  speed: number;
  scrollY: number;
  hwnd: number;
  status: string;
};

async function barState(client: ControlClient): Promise<BarState> {
  const res = await client.request(ControlCommand.TestReadingAutoScroll, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  const scroll = /scrollY=(-?\d+)/.exec(out);
  const scrollY = scroll ? +scroll[1]! : -1;
  if (exitCode === 2) {
    return { visible: false, paused: 0, dir: 0, speed: 0, scrollY, hwnd: 0, status: "" };
  }
  if (exitCode !== 0) {
    throw new Error(`issue-6169: TestReadingAutoScroll failed: ${out.trim()}`);
  }
  const ok = /OK visible=1 paused=(\d+) atEnd=\d+ dir=(-?\d+) speed=(\d+) scrollY=(-?\d+).*hwnd=(-?\d+)/.exec(out);
  const status = /status=(.*)/.exec(out);
  if (!ok) {
    throw new Error(`issue-6169: could not parse: ${out.trim()}`);
  }
  return {
    visible: true,
    paused: +ok[1]!,
    dir: +ok[2]!,
    speed: +ok[3]!,
    scrollY: +ok[4]!,
    hwnd: +ok[5]!,
    status: status ? status[1]!.trim() : "",
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
  throw new Error(`issue-6169: ${msg}: ${JSON.stringify(last)}`);
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

    sendCommand(frame, cmdId("CmdToggleAutomaticallyScroll"));
    let st = await waitBar(client, (s) => s.visible && s.paused === 0, "bar did not show");
    const startY = st.scrollY;
    console.log(`  started at scrollY=${startY} speed=${st.speed}`);

    st = await waitBar(client, (s) => s.visible && s.scrollY > startY, "document did not auto-scroll", 2500);
    console.log(`  scrolled to ${st.scrollY} ✓`);

    postKey(frame, VK_SPACE);
    st = await waitBar(client, (s) => s.visible && s.paused === 1, "Space did not pause");
    const pausedY = st.scrollY;
    await sleep(300);
    st = await barState(client);
    if (!st.visible || st.paused !== 1 || st.scrollY !== pausedY) {
      throw new Error(`issue-6169: kept moving while paused: ${JSON.stringify(st)}`);
    }
    console.log(`  Space paused at ${pausedY} ✓`);

    const speedBefore = st.speed;
    postKey(frame, VK_DOWN);
    st = await waitBar(client, (s) => s.visible && s.speed > speedBefore, "Down did not increase speed");
    console.log(`  Down: ${speedBefore} -> ${st.speed} px/s ✓`);

    sendCommand(frame, cmdId("CmdPrevTab"));
    st = await waitBar(client, (s) => !s.visible, "bar still shown on Home");
    await sleep(300);
    st = await barState(client);
    if (st.visible) {
      throw new Error(`issue-6169: bar reappeared on Home: ${JSON.stringify(st)}`);
    }
    console.log(`  Home tab hid the bar ✓`);

    sendCommand(frame, cmdId("CmdNextTab"));
    st = await waitBar(client, (s) => s.visible && s.paused === 1, "bar did not return with the document tab");
    console.log(`  document tab restored the paused bar ✓`);

    postKey(frame, VK_ESCAPE);
    st = await waitBar(client, (s) => !s.visible, "Esc did not hide the bar");
    console.log(`  Esc stopped auto-scroll ✓`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
