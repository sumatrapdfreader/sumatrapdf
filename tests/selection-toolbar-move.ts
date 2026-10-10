// The floating selection toolbar is a WS_POPUP in screen coordinates, so it
// must be moved with the frame (WM_MOVE). Without that it stays on the old
// screen spot while / after the window is dragged.
//
// Run: bun tests/selection-toolbar-move.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, makeTextPdf } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync, pressVKey } from "./win-automation.ts";
import { SWP_NOACTIVATE, SWP_NOZORDER, getWindowRect, moveWindow, postChar, setWindowPos, sleep } from "./winapi.ts";

const VK_END = 0x23;

const LINE = "The quick brown fox jumps over the lazy dog";

function writeAppData(dir: string): string {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "CheckForUpdates = false", "ShowToc = false", "SelectionToolbar = true", ""].join("\n"),
  );
  return dir;
}

function toolbarVisible(raw: string): boolean {
  return /^visible=1$/m.test(raw);
}

function parsePlaced(raw: string): { x: number; y: number; dx: number; dy: number } {
  const m = /^placed=(-?\d+),(-?\d+),(-?\d+),(-?\d+)$/m.exec(raw);
  if (!m) {
    throw new Error(`selection-toolbar-move: no placed= in:\n${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
}

async function waitForToolbarVisible(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await toolbarDump(client);
    if (toolbarVisible(raw)) {
      return raw;
    }
    await sleep(40);
  }
  throw new Error(`selection-toolbar-move: selection toolbar did not appear\n${raw}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("selection-toolbar-move.pdf");
  writeFileSync(pdf, makeTextPdf(LINE));
  const dir = writeAppData(tmpPath("selection-toolbar-move-appdata"));

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
    const startDeadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    let dump = "";
    while (Date.now() < startDeadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/active=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/active=1/.test(dump)) {
      throw new Error(`selection-toolbar-move: keyboard selection did not start\n${dump}`);
    }

    await postChar(frame, "v");
    while (Date.now() < startDeadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/visual=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/visual=1/.test(dump)) {
      throw new Error(`selection-toolbar-move: visual mode did not start\n${dump}`);
    }

    pressVKey(frame, VK_END);
    const raw0 = await waitForToolbarVisible(client);
    const tb0 = parsePlaced(raw0);
    const frame0 = getWindowRect(frame);

    // several SetWindowPos calls, each generating WM_MOVE: this is the "while
    // moving" path (live drag) as well as the "after" snap at the last step
    const steps = [
      { dx: -48, dy: 0 },
      { dx: -48, dy: 36 },
      { dx: 0, dy: 36 },
    ];
    let originX = frame0.left;
    let originY = frame0.top;
    const width = frame0.right - frame0.left;
    const height = frame0.bottom - frame0.top;
    for (const step of steps) {
      originX += step.dx;
      originY += step.dy;
      if (IS_MAC) {
        if (!moveWindow(frame, originX, originY, width, height)) {
          throw new Error(`selection-toolbar-move: move failed at ${originX},${originY}`);
        }
      } else if (!setWindowPos(frame, originX, originY, width, height, SWP_NOZORDER | SWP_NOACTIVATE)) {
        throw new Error(`selection-toolbar-move: SetWindowPos failed at ${originX},${originY}`);
      }
      const raw = await toolbarDump(client);
      if (!toolbarVisible(raw)) {
        throw new Error(`selection-toolbar-move: toolbar hid after move to ${originX},${originY}\n${raw}`);
      }
      const tb = parsePlaced(raw);
      const frameNow = getWindowRect(frame);
      if (IS_MAC) {
        // placed= is the card inside the frame, not a screen point. A card
        // left on the old screen spot shifts placed by about the frame delta.
        const frameDx = frameNow.left - frame0.left;
        const frameDy = frameNow.top - frame0.top;
        if (frameDx === 0 && frameDy === 0) {
          throw new Error(`selection-toolbar-move: frame did not move from ${frame0.left},${frame0.top}\n${raw}`);
        }
        const tbDx = tb.x - tb0.x;
        const tbDy = tb.y - tb0.y;
        if (Math.abs(tbDx) > 40 || Math.abs(tbDy) > 40) {
          throw new Error(
            `selection-toolbar-move: toolbar left its place in the frame ` +
              `(frame ${frameDx},${frameDy} placed ${tbDx},${tbDy})\n${raw}`,
          );
        }
        if (tb.dx !== tb0.dx || tb.dy !== tb0.dy) {
          throw new Error(
            `selection-toolbar-move: toolbar size changed ${tb.dx},${tb.dy} vs ${tb0.dx},${tb0.dy}\n${raw}`,
          );
        }
        continue;
      }
      const frameDx = frameNow.left - frame0.left;
      const frameDy = frameNow.top - frame0.top;
      const tbDx = tb.x - tb0.x;
      const tbDy = tb.y - tb0.y;
      if (tbDx !== frameDx || tbDy !== frameDy) {
        throw new Error(
          `selection-toolbar-move: toolbar did not follow the frame ` +
            `(frame ${frameDx},${frameDy} toolbar ${tbDx},${tbDy}) ` +
            `at ${originX},${originY}\n${raw}`,
        );
      }
      if (tb.dx !== tb0.dx || tb.dy !== tb0.dy) {
        throw new Error(
          `selection-toolbar-move: toolbar size changed ${tb.dx},${tb.dy} vs ${tb0.dx},${tb0.dy}\n${raw}`,
        );
      }
    }

    console.log("selection-toolbar-move: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
