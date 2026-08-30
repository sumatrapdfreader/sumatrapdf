// #6107: Read Aloud bar uses a compact speed slider (circle on a bar + label)
// with 0.25 steps from 2x to 3.5x.
//
// Run: bun tests/issue-6107.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";
import { MK_LBUTTON, packCoords, sendMessage, sleep, WM_LBUTTONDOWN, WM_LBUTTONUP } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type BarState = {
  voices: number;
  speaking: number;
  visible: boolean;
  hwnd: number;
  speed: { x: number; y: number; dx: number; dy: number };
  speedIdx: number;
  speedCount: number;
  label: string;
};

async function barState(client: ControlClient): Promise<BarState | null> {
  const res = await client.request(ControlCommand.TestReadAloudPlaybackBar, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  const voices = /voices=(\d+)/.exec(out);
  const speaking = /speaking=(\d+)/.exec(out);
  if (!voices || !speaking) {
    throw new Error(`issue-6107: could not parse: ${out.trim()}`);
  }
  if (exitCode === 2) {
    return {
      voices: +voices[1]!,
      speaking: +speaking[1]!,
      visible: false,
      hwnd: 0,
      speed: { x: 0, y: 0, dx: 0, dy: 0 },
      speedIdx: -1,
      speedCount: 0,
      label: "",
    };
  }
  if (exitCode !== 0) {
    throw new Error(`issue-6107: TestReadAloudPlaybackBar failed: ${out.trim()}`);
  }
  const ok = /OK visible=1 resume=\d+ hwnd=(-?\d+)/.exec(out);
  const speed = /speed=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(out);
  const idx = /speedIdx=(-?\d+) speedCount=(\d+) label=(\S+)/.exec(out);
  if (!ok || !speed || !idx) {
    throw new Error(`issue-6107: missing slider fields: ${out.trim()}`);
  }
  return {
    voices: +voices[1]!,
    speaking: +speaking[1]!,
    visible: true,
    hwnd: +ok[1]!,
    speed: { x: +speed[1]!, y: +speed[2]!, dx: +speed[3]!, dy: +speed[4]! },
    speedIdx: +idx[1]!,
    speedCount: +idx[2]!,
    label: idx[3]!,
  };
}

function clickSpeed(st: BarState, t: number): void {
  const x = st.speed.x + Math.max(2, Math.floor(st.speed.dx * t));
  const y = st.speed.y + Math.floor(st.speed.dy / 2);
  const lp = packCoords(x, y);
  sendMessage(st.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  sendMessage(st.hwnd, WM_LBUTTONUP, 0, lp);
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    let st = await barState(client);
    if (!st || st.voices === 0) {
      console.log("SKIP issue-6107: no TTS voices on this machine");
      return;
    }

    sendCommand(frame, cmdId("CmdReadAloud"));
    const startDeadline = Date.now() + 12_000 * SLOW_BUILD_FACTOR;
    for (;;) {
      st = await barState(client);
      if (st && st.visible && st.speed.dx > 0 && st.speedCount >= 12) {
        break;
      }
      if (Date.now() > startDeadline) {
        throw new Error(`issue-6107: read-aloud bar never showed a slider: ${JSON.stringify(st)}`);
      }
      await sleep(80);
    }

    if (st.speedCount !== 12) {
      throw new Error(`issue-6107: expected 12 speeds, got ${st.speedCount}`);
    }
    const startIdx = st.speedIdx;
    clickSpeed(st, 0.92);
    const moveDeadline = Date.now() + 3_000 * SLOW_BUILD_FACTOR;
    for (;;) {
      st = await barState(client);
      if (st && st.visible && st.speedIdx > startIdx) {
        break;
      }
      if (Date.now() > moveDeadline) {
        throw new Error(`issue-6107: slider did not raise speed from ${startIdx}: ${JSON.stringify(st)}`);
      }
      await sleep(40);
    }
    if (!/^\d+(\.\d+)?x$/.test(st.label)) {
      throw new Error(`issue-6107: bad speed label '${st.label}'`);
    }
    sendCommand(frame, cmdId("CmdStopReadAloud"));
    console.log("issue-6107: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
