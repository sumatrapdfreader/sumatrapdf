// Test for issue #6579: in keyboard selection mode (F7) the annotation
// commands act on the annotation under the caret, not the mouse pointer.
// Ctrl + Delete (CmdDeleteAnnotation) deletes the highlight the caret is on.
//
// Run: bun tests/issue-6579.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { VK_END, VK_HOME, WM_KEYDOWN, WM_KEYUP, postChar, postMessage, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const name = "issue-6579";

type State = { annotations: number; selected: boolean; raw: string };

async function state(client: ControlClient): Promise<State> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const sel = /state selected=(\d+)/.exec(raw);
  if (!count || !sel) {
    throw new Error(`${name}: could not read annotations\n${raw}`);
  }
  return { annotations: +count[1]!, selected: sel[1] === "1", raw };
}

async function waitForState(client: ControlClient, pred: (s: State) => boolean, what: string): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await state(client);
    if (pred(s)) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`${name}: ${what}\n${s.raw}`);
    }
    await sleep(40);
  }
}

async function caretDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
}

async function waitCaret(client: ControlClient, re: RegExp, what: string): Promise<void> {
  const deadline = Date.now() + 4_000 * SLOW_BUILD_FACTOR;
  let dump = "";
  while (Date.now() < deadline) {
    dump = await caretDump(client);
    if (re.test(dump)) {
      return;
    }
    await sleep(25);
  }
  throw new Error(`${name}: ${what}\n${dump}`);
}

async function pressKey(frame: number, vk: number): Promise<void> {
  postMessage(frame, WM_KEYDOWN, vk, 0);
  postMessage(frame, WM_KEYUP, vk, 0);
  await sleep(150);
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // highlight the first line with the keyboard: F7, v, End
    sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
    await waitCaret(client, /active=1/, "keyboard selection did not start");
    await postChar(frame, "v");
    await waitCaret(client, /visual=1/, "visual mode did not start");
    await pressKey(frame, VK_END);
    sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));
    let s = await waitForState(client, (st) => st.annotations === 1, "highlight was not created");

    // the caret on the highlight, nothing selected, the mouse elsewhere
    await waitCaret(client, /active=1/, "keyboard selection ended after the highlight");
    await pressKey(frame, VK_HOME);
    s = await state(client);
    if (s.selected) {
      throw new Error(`${name}: the highlight is selected, so Delete would not need the caret\n${s.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdDeleteAnnotation"));
    await waitForState(client, (st) => st.annotations === 0, "Ctrl + Delete didn't delete the highlight at the caret");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
