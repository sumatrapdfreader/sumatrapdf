// Automatically Scroll extras: the bar shows the time left to the end of the
// document, ReadingAutoScrollMinutes pauses scrolling when it runs out,
// minimizing pauses (restoring resumes) and the speed is remembered per
// document.
//
// Run: bun tests/auto-scroll-extras.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import {
  postMessage,
  showWindow,
  sleep,
  SW_SHOWMINNOACTIVE,
  SW_SHOWNOACTIVATE,
  VK_DOWN,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi.ts";

const kSettings = "SumatraPDF-settings.txt";
// 6 seconds, so the timer runs out quickly
const kMinutes = 0.1;

type BarState = { visible: boolean; paused: number; speed: number; scrollY: number; status: string };

async function barState(client: ControlClient): Promise<BarState> {
  const res = await client.request(ControlCommand.TestReadingAutoScroll, []);
  const out = String(res[1] ?? "");
  const ok = /OK visible=1 paused=(\d+) .*speed=(\d+)/.exec(out);
  const status = /status=(.*)/.exec(out);
  const scroll = /scrollY=(-?\d+)/.exec(out);
  const scrollY = scroll ? +scroll[1]! : -1;
  if (!ok) {
    return { visible: false, paused: 0, speed: 0, scrollY, status: "" };
  }
  return { visible: true, paused: +ok[1]!, speed: +ok[2]!, scrollY, status: status ? status[1]!.trim() : "" };
}

async function waitBar(client: ControlClient, pred: (s: BarState) => boolean, msg: string, ms = 6000) {
  const deadline = Date.now() + ms;
  let last = await barState(client);
  while (!pred(last)) {
    if (Date.now() > deadline) {
      throw new Error(`auto-scroll-extras: ${msg}: ${JSON.stringify(last)}`);
    }
    await sleep(100);
    last = await barState(client);
  }
  return last;
}

async function startScroll(client: ControlClient): Promise<BarState> {
  await client.waitForRenderIdle();
  await client.setNotificationsEnabled(false);
  await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleAutomaticallyScroll"]);
  return waitBar(client, (s) => s.visible && s.paused === 0, "auto-scroll did not start");
}

function writeSettings(dir: string, globalSpeed: number): void {
  const s = readFileSync(join(dir, kSettings), "utf8");
  writeFileSync(
    join(dir, kSettings),
    s.replace(/^ReadingAutoScrollSpeed = .*$/m, `ReadingAutoScrollSpeed = ${globalSpeed}`),
  );
}

export async function testit(): Promise<void> {
  const dir = tmpPath("auto-scroll-extras-appdata");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, kSettings),
    `ReadingAutoScrollSpeed = 40\nReadingAutoScrollMinutes = ${kMinutes}\nCheckForUpdates = false\nRestoreSession = false\n`,
  );
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  let docSpeed = 0;

  // run 1: time left, minimize pause, timer, then change the speed and quit
  let { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    let st = await startScroll(client);
    if (!/ left/.test(st.status)) {
      throw new Error(`auto-scroll-extras: status should show the time left: "${st.status}"`);
    }
    console.log(`  status: ${st.status}`);

    // the bar hides with the minimized window, so watch the scroll position
    showWindow(frame, SW_SHOWMINNOACTIVE);
    await sleep(300);
    const minY = (await barState(client)).scrollY;
    await sleep(800);
    const minY2 = (await barState(client)).scrollY;
    if (minY !== minY2) {
      throw new Error(`auto-scroll-extras: kept scrolling while minimized: ${minY} -> ${minY2}`);
    }
    showWindow(frame, SW_SHOWNOACTIVATE);
    st = await waitBar(client, (s) => s.visible && s.paused === 0, "restoring did not resume");
    await waitBar(client, (s) => s.scrollY > st.scrollY, "did not scroll after restoring");
    console.log(`  minimize paused, restore resumed`);

    st = await waitBar(client, (s) => s.paused === 1 && /Time is up/.test(s.status), "the timer did not pause", 12000);
    console.log(`  timer: ${st.status}`);

    postMessage(frame, WM_KEYDOWN, VK_DOWN, 1);
    postMessage(frame, WM_KEYUP, VK_DOWN, 1);
    st = await waitBar(client, (s) => s.speed > 40, "Down did not change the speed");
    docSpeed = st.speed;
    await client.quit();
    await proc.exited;
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const saved = readFileSync(join(dir, kSettings), "utf8");
  const m = /^\s+ReadingAutoScrollSpeed = (\d+)/m.exec(saved);
  if (!m || +m[1]! !== docSpeed) {
    throw new Error(`auto-scroll-extras: the document's speed ${docSpeed} was not saved: ${m?.[0] ?? "(missing)"}`);
  }

  // run 2: another global speed; the document keeps its own
  writeSettings(dir, 2);
  ({ proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true }));
  try {
    const st = await startScroll(client);
    if (st.speed !== docSpeed) {
      throw new Error(`auto-scroll-extras: expected the document's speed ${docSpeed}, got ${st.speed}`);
    }
    console.log(`  per-document speed ${docSpeed} px/s restored`);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
