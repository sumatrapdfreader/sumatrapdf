// discussion #5922: extend an existing text selection from the keyboard.
// CmdExtendSelectionCharLeft / CharRight / WordLeft / WordRight move the free
// end of the selection by a character or a word. They have no default shortcut
// (Ctrl + Shift + Left/Right navigate between files), so they're driven here as
// commands, which is also how a user-assigned shortcut reaches them.
//
// Covers both paths: while F7 keyboard selection is on the commands move the
// caret, and after leaving that mode they move the selection's end the same way
// they do for a selection made with the mouse.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, makeTextPdf } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { WM_KEYDOWN, WM_KEYUP, postMessage, sleep, waitForTopWindow, postChar } from "./winapi";

const VK_RIGHT = 0x27;

const LINE = "The quick brown fox jumps over the lazy dog";

type State = { active: boolean; selRects: number; text: string; dump: string };

async function getState(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestSelectTextKeyboard, []);
  const dump = String(res[1] ?? "");
  const m = /active=(\d+) .* selRects=(\d+)/.exec(dump);
  if (!m) {
    throw new Error(`unexpected TestSelectTextKeyboard output:\n${dump}`);
  }
  const text = /^text=(.*)$/m.exec(dump);
  return { active: m[1] === "1", selRects: +m[2]!, text: text ? text[1]! : "", dump };
}

async function waitForState(
  client: ControlClient,
  pred: (s: State) => boolean,
  timeoutMs = 4000 * SLOW_BUILD_FACTOR,
): Promise<State> {
  const deadline = Date.now() + timeoutMs;
  let last: State = { active: false, selRects: 0, text: "", dump: "" };
  while (Date.now() < deadline) {
    last = await getState(client);
    if (pred(last)) {
      return last;
    }
    await sleep(25);
  }
  throw new Error(`selection state did not match in time\n${last.dump}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5922.pdf");
  writeFileSync(pdf, makeTextPdf(LINE));

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();

      const fail = (msg: string, st: State) => {
        throw new Error(`${msg}\nstate dump:\n${st.dump}`);
      };
      const extend = async (cmd: string, want: string) => {
        sendCommandSync(frame, cmdId(cmd));
        return waitForState(client, (s) => s.text === want);
      };
      const expectText = (st: State, want: string, what: string) => {
        if (st.text !== want) {
          fail(`${what}: selection should be "${want}", is "${st.text}"`, st);
        }
      };

      // make a selection: F7 caret browsing, visual mode, 9 glyphs right
      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      await waitForState(client, (s) => s.active);
      await postChar(frame, "v");
      for (let i = 0; i < 9; i++) {
        postMessage(frame, WM_KEYDOWN, VK_RIGHT, 0);
        postMessage(frame, WM_KEYUP, VK_RIGHT, 0);
      }
      let st = await waitForState(client, (s) => s.text === LINE.slice(0, 9));
      expectText(st, LINE.slice(0, 9), "setup");

      // --- with keyboard selection active: the commands move the caret ---
      st = await extend("CmdExtendSelectionWordRight", LINE.slice(0, 15));
      expectText(st, LINE.slice(0, 15), "word right (caret mode)"); // "The quick brown"

      st = await extend("CmdExtendSelectionCharRight", LINE.slice(0, 16));
      expectText(st, LINE.slice(0, 16), "char right (caret mode)");

      st = await extend("CmdExtendSelectionCharLeft", LINE.slice(0, 15));
      expectText(st, LINE.slice(0, 15), "char left (caret mode)");

      st = await extend("CmdExtendSelectionWordLeft", LINE.slice(0, 10));
      expectText(st, LINE.slice(0, 10), "word left (caret mode)"); // back to "The quick "

      // --- leave the mode; the selection stays and is still extendable, which
      // is the case from the discussion (a selection made with the mouse) ---
      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      st = await waitForState(client, (s) => !s.active);
      if (st.active) {
        fail("F7 should have turned keyboard selection off", st);
      }
      if (st.selRects === 0) {
        fail("leaving keyboard selection should keep the selection", st);
      }
      expectText(st, LINE.slice(0, 10), "after leaving caret mode");

      st = await extend("CmdExtendSelectionWordRight", LINE.slice(0, 15));
      expectText(st, LINE.slice(0, 15), "word right (plain selection)");

      st = await extend("CmdExtendSelectionCharRight", LINE.slice(0, 16));
      expectText(st, LINE.slice(0, 16), "char right (plain selection)");

      st = await extend("CmdExtendSelectionWordLeft", LINE.slice(0, 10));
      expectText(st, LINE.slice(0, 10), "word left (plain selection)");

      st = await extend("CmdExtendSelectionCharLeft", LINE.slice(0, 9));
      expectText(st, LINE.slice(0, 9), "char left (plain selection)");
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
