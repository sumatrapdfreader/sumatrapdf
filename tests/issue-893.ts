// #893: find remembers the last 10 search queries in memory and offers them
// from the find field's drop-down. Not persisted.
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR } from "./util";
import { getClassName, getControlText, getParentWindow, sendMessage } from "./winapi";
import {
  launchControlled,
  pressKey,
  sendCommand,
  typeIntoInput,
  waitForFocusClass,
  killAndWait,
} from "./win-automation";

const VK_RETURN = 0x0d;
const EM_GETSEL = 0x00b0;
const EM_SETSEL = 0x00b1;

function comboOfEdit(edit: number): number {
  const parent = getParentWindow(edit);
  return parent && getClassName(parent) === "ComboBox" ? parent : edit;
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit = await waitForFocusClass(frame, "Edit");
    const combo = comboOfEdit(edit);
    await typeIntoInput(combo, "deflate", false);
    await pressKey(edit, VK_RETURN);
    await waitHistory(client, (lines) => lines[0] === "deflate");

    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit2 = await waitForFocusClass(frame, "Edit");
    const combo2 = comboOfEdit(edit2);
    await typeIntoInput(combo2, "inflate", false);
    sendMessage(edit2, EM_SETSEL, 7, 7); // caret at end
    await pressKey(edit2, VK_RETURN, 250 * SLOW_BUILD_FACTOR);

    const hist = await waitHistory(client, (lines) => lines[0] === "inflate" && lines.includes("deflate"));
    if (hist[0] !== "inflate" || hist[1] !== "deflate") {
      throw new Error(`issue-893: expected inflate then deflate, got ${JSON.stringify(hist)}`);
    }

    // Read the combo/edit, not the thread's focused HWND: after Enter a suite
    // run often has no GUI focus (GetGUIThreadInfo hwndFocus is 0).
    const afterEnter = getControlText(combo2) || getControlText(edit2);
    if (afterEnter !== "inflate") {
      throw new Error(`issue-893: Enter cleared the find field, got '${afterEnter}'`);
    }
    const sel = Number(sendMessage(edit2, EM_GETSEL, 0, 0));
    const caret = sel & 0xffff;
    if (caret !== 7) {
      throw new Error(`issue-893: Enter moved the caret to ${caret}, want 7`);
    }
    console.log("issue-893: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

async function waitHistory(client: ControlClient, pred: (lines: string[]) => boolean): Promise<string[]> {
  const deadline = Date.now() + 8000 * SLOW_BUILD_FACTOR;
  let last: string[] = [];
  for (;;) {
    const res = await client.request(ControlCommand.TestFindHistory, []);
    const raw = String(res[1] ?? "").trim();
    last = raw.length === 0 ? [] : raw.split("\n");
    if (pred(last)) {
      return last;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-893: history never matched (last: ${JSON.stringify(last)})`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
