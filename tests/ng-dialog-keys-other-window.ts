// ng: a dialog drawn in one window (tool windows off, as for a message box on
// macOS / Linux) must not take the keys of another. That window's shortcuts
// were all turned off, and Enter / Esc there answered the dialog.
//
// Run: bun tests/ng-dialog-keys-other-window.ts -ng [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled } from "./win-automation.ts";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  postMessage,
  sleep,
  VK_ESCAPE,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi.ts";

const kAccelsAll = "0";
const kAccelsNone = "2";

async function frameIds(client: ControlClient): Promise<number[]> {
  const res = await client.request(ControlCommand.TestWindows, []);
  const out: number[] = [];
  for (const line of String(res[1] ?? "").split("\n")) {
    const m = /^hwnd=(\d+) /.exec(line);
    if (m && !line.includes(" name=")) {
      out.push(+m[1]!);
    }
  }
  return out;
}

// runs fn with the app's requests aimed at frame id
async function inFrame<T>(client: ControlClient, id: number, fn: () => Promise<T>): Promise<T> {
  await client.request(ControlCommand.TestWindows, ["target", id]);
  try {
    return await fn();
  } finally {
    await client.request(ControlCommand.TestWindows, ["target", 0]);
  }
}

async function accels(client: ControlClient, id: number): Promise<string> {
  const res = await inFrame(client, id, () => client.request(ControlCommand.TestToolWindow, ["dialog-accels"]));
  return /accels=(-?\d+)/.exec(String(res[1] ?? ""))?.[1] ?? "";
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP ng-dialog-keys-other-window: ng's dialogs");
    return;
  }
  const pdf = tmpPath("ng-dialog-keys-other-window.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestToolWindow, ["off"]);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdDuplicateInNewWindow"]);
    const frames = await pollUntil(
      () => frameIds(client),
      (fs) => fs.length === 2,
      { error: (fs) => `ng-dialog-keys-other-window: want 2 frames, got ${fs.length}` },
    );
    const [a, b] = [frames[0]!, frames[1]!];

    await inFrame(client, a, () => client.request(ControlCommand.TestInvokeCommand, ["CmdChangeScrollbar"]));
    await pollUntil(
      () => accels(client, a),
      (v) => v === kAccelsNone,
      { error: (v) => `ng-dialog-keys-other-window: accels=${v} in the dialog's window, want ${kAccelsNone}` },
    );
    const inB = await accels(client, b);
    console.log(`ng-dialog-keys-other-window: accels in the other window: ${inB}`);
    if (inB !== kAccelsAll) {
      throw new Error(`ng-dialog-keys-other-window: the other window lost its shortcuts: accels=${inB}`);
    }

    // macOS has no window handle to post keys to
    if (IS_MAC) {
      return;
    }
    let frameB = 0;
    enumWindows((h) => {
      if (getWindowPid(h) === proc.pid && getClassName(h) === FRAME_CLASS && h !== frame) {
        frameB = h;
      }
      return true;
    });
    if (!frameB) {
      throw new Error("ng-dialog-keys-other-window: no second frame window");
    }
    for (const vk of [VK_RETURN, VK_ESCAPE]) {
      postMessage(frameB, WM_KEYDOWN, vk, 0);
      await sleep(300);
      const after = await accels(client, a);
      if (after !== kAccelsNone) {
        throw new Error(`ng-dialog-keys-other-window: key ${vk} in the other window closed the dialog`);
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
