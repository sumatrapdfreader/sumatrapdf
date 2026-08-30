// #6106: the floating read-aloud bar must take Pause while speech is running.
// TTS word events used to relayout the bar and eat mouse-up, so Pause/Stop/Speed
// did nothing; Resume worked because those events stop when paused.
//
// Run: bun tests/issue-6106.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone } from "./util.ts";
import { MK_LBUTTON, packCoords, sendMessage, sleep, WM_LBUTTONDOWN, WM_LBUTTONUP } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type BarState = {
  voices: number;
  speaking: number;
  visible: boolean;
  resume: number;
  hwnd: number;
  pause: { x: number; y: number; dx: number; dy: number };
};

async function barState(client: ControlClient): Promise<BarState | null> {
  const res = await client.request(ControlCommand.TestReadAloudPlaybackBar, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  const voices = /voices=(\d+)/.exec(out);
  const speaking = /speaking=(\d+)/.exec(out);
  if (!voices || !speaking) {
    throw new Error(`issue-6106: could not parse: ${out.trim()}`);
  }
  if (exitCode === 2) {
    return {
      voices: +voices[1]!,
      speaking: +speaking[1]!,
      visible: false,
      resume: 0,
      hwnd: 0,
      pause: { x: 0, y: 0, dx: 0, dy: 0 },
    };
  }
  if (exitCode !== 0) {
    throw new Error(`issue-6106: TestReadAloudPlaybackBar failed: ${out.trim()}`);
  }
  const ok = /OK visible=1 resume=(\d+) hwnd=(-?\d+)/.exec(out);
  const pause = /pause=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(out);
  if (!ok || !pause) {
    throw new Error(`issue-6106: missing bar fields: ${out.trim()}`);
  }
  return {
    voices: +voices[1]!,
    speaking: +speaking[1]!,
    visible: true,
    resume: +ok[1]!,
    hwnd: +ok[2]!,
    pause: { x: +pause[1]!, y: +pause[2]!, dx: +pause[3]!, dy: +pause[4]! },
  };
}

function clickPause(st: BarState): void {
  const x = st.pause.x + Math.floor(st.pause.dx / 2);
  const y = st.pause.y + Math.floor(st.pause.dy / 2);
  const lp = packCoords(x, y);
  sendMessage(st.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
}

export async function testit(): Promise<void> {
  // SAPI posts a window message per word; WinRT does not. That is the path
  // that relayouts the bar and eats mouse-up.
  process.env.SUMATRA_TTS_FORCE_SAPI = "1";
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    let st = await barState(client);
    if (!st || st.voices === 0) {
      console.log("SKIP issue-6106: no TTS voices on this machine");
      return;
    }

    sendCommand(frame, cmdId("CmdReadAloud"));
    const startDeadline = Date.now() + 12_000;
    for (;;) {
      st = await barState(client);
      if (st && st.visible && st.speaking === 1 && st.pause.dx > 0) {
        break;
      }
      if (Date.now() > startDeadline) {
        throw new Error(`issue-6106: read-aloud bar never started speaking: ${JSON.stringify(st)}`);
      }
      await sleep(80);
    }

    // let SAPI word-boundary events start arriving
    await sleep(400);
    st = await barState(client);
    if (!st || !st.visible || st.speaking !== 1) {
      throw new Error(`issue-6106: speech stopped before Pause: ${JSON.stringify(st)}`);
    }

    // leave a gap between down and up so a SAPI word-boundary can relayout
    clickPause(st);
    await sleep(250);
    sendMessage(
      st.hwnd,
      WM_LBUTTONUP,
      0,
      packCoords(st.pause.x + Math.floor(st.pause.dx / 2), st.pause.y + Math.floor(st.pause.dy / 2)),
    );

    const pauseDeadline = Date.now() + 3_000;
    for (;;) {
      st = await barState(client);
      if (st && st.speaking === 0) {
        break;
      }
      if (Date.now() > pauseDeadline) {
        throw new Error(`issue-6106: Pause on the playback bar did not stop speech: ${JSON.stringify(st)}`);
      }
      await sleep(50);
    }
    sendCommand(frame, cmdId("CmdStopReadAloud"));
    console.log("issue-6106: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
