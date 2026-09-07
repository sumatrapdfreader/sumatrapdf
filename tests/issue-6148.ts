// issue #6148: on the IE backend (Windows 7 has no WebView2) neither the mouse
// wheel nor the arrow keys scrolled a CHM or markdown document; only the
// browser's own scrollbars worked.
//
// Two causes, both in HtmlWindow:
//   - GetBrowserControlHwnd() descended into the first child at each level to
//     reach the "Internet Explorer_Server" window. Our canvas hosts an edit
//     control that comes first, so the walk returned null and SendMsg() sent
//     every forwarded wheel/key/scroll message to nowhere.
//   - the canvas subclass swallowed WM_MOUSEWHEEL instead of forwarding it, so
//     a wheel landing on the parent (the usual routing when the browser isn't
//     focused) did nothing even once the hwnd was right.
//
// CHM shares both code paths; markdown is what the test can read a scroll
// position out of.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { findCanvas, launchControlled, killAndWait, ensureModifierKeysUp } from "./win-automation.ts";
import { clientToScreen, getClientRect, packCoords, sendMessage, setCursorPos, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WM_KEYDOWN = 0x0100;
const WM_KEYUP = 0x0101;
const VK_DOWN = 0x28;
const WHEEL_DELTA = 120;
const NOTCHES = 8;

async function scrollY(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 8000;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [0, 0]);
    last = String(res[1] ?? "").trim();
    const m = /^OK scrollX=\d+ scrollY=(-?\d+)/.exec(last);
    if (res[0] === 0 && m && Number(m[1]) >= 0) {
      return Number(m[1]);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6148: no scroll position reported: ${last}`);
    }
    await sleep(80);
  }
}

async function scrolledFrom(client: ControlClient, before: number): Promise<number> {
  const deadline = Date.now() + 4000;
  let pos = before;
  while (Date.now() < deadline) {
    pos = await scrollY(client);
    if (pos > before) {
      return pos;
    }
    await sleep(80);
  }
  return pos;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6148");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const paragraphs = Array.from({ length: 200 }, (_, i) => `Paragraph ${i + 1}: enough text to overflow the window.`);
  writeFileSync(join(dir, "long.md"), ["# Start", ...paragraphs].join("\n\n"));
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["MarkdownUI [", "\tUseFixedPageUI = false", "]", "RestoreSession = false", "ShowStartPage = false", ""].join("\n"),
  );

  // -html-backend ie: on a machine with WebView2 installed this is the only way
  // to reach the code path Windows 7 always takes
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-html-backend",
    "ie",
    join(dir, "long.md"),
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6148: no canvas");
    }
    await ensureModifierKeysUp();
    const cr = getClientRect(canvas);
    const mid = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    setCursorPos(mid.x, mid.y);
    const lp = packCoords(mid.x, mid.y);
    const wp = (-WHEEL_DELTA << 16) >>> 0;

    // the wheel lands on the canvas when the browser control isn't focused
    let before = await scrollY(client);
    for (let i = 0; i < NOTCHES; i++) {
      sendMessage(canvas, WM_MOUSEWHEEL, wp, lp);
    }
    let after = await scrolledFrom(client, before);
    if (after <= before) {
      throw new Error(`issue-6148: wheel on the canvas did not scroll (${before} -> ${after})`);
    }
    console.log(`issue-6148: wheel on canvas ${before} -> ${after}`);

    // and on the frame when it is focused; the frame forwards it via PassUIMsg
    before = after;
    for (let i = 0; i < NOTCHES; i++) {
      sendMessage(frame, WM_MOUSEWHEEL, wp, lp);
    }
    after = await scrolledFrom(client, before);
    if (after <= before) {
      throw new Error(`issue-6148: wheel on the frame did not scroll (${before} -> ${after})`);
    }
    console.log(`issue-6148: wheel on frame ${before} -> ${after}`);

    before = after;
    for (let i = 0; i < NOTCHES; i++) {
      sendMessage(frame, WM_KEYDOWN, VK_DOWN, 0);
      sendMessage(frame, WM_KEYUP, VK_DOWN, 0);
    }
    after = await scrolledFrom(client, before);
    if (after <= before) {
      throw new Error(`issue-6148: arrow keys did not scroll (${before} -> ${after})`);
    }
    console.log(`issue-6148: arrow keys ${before} -> ${after}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6148: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
