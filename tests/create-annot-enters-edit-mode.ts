// CmdCreateAnnot* turns on Edit PDF mode only when the command has `openedit`.
// Shift+A is CmdCreateAnnotHighlight openedit. Paste still enters the mode.

import { mkdirSync, writeFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  VK_END,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_KEYUP,
  packCoords,
  postChar,
  postMessage,
  sendMessage,
  sleep,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

type State = { editToolbar: boolean; annotations: number; raw: string };

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const toolbar = /state selected=\d+ hover=\d+ editToolbar=(\d)/.exec(raw);
  const count = /annotations=(\d+)/.exec(raw);
  if (res[0] !== 0 || !toolbar || !count) {
    throw new Error(`create-annot-enters-edit-mode: could not read state\n${raw}`);
  }
  return { editToolbar: toolbar[1] === "1", annotations: +count[1]!, raw };
}

async function waitForState(client: ControlClient, pred: (s: State) => boolean, what: string): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await state(client);
    if (pred(s)) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`create-annot-enters-edit-mode: ${what}\n${s.raw}`);
    }
    await sleep(40);
  }
}

// Select a line of text without the mouse: injected mouse input is dropped on
// this machine, so the keyboard selection mode is the only way to get a real
// text selection (a rectangular Select All does not mark up text).
async function selectLineWithKeyboard(client: ControlClient, frame: number): Promise<void> {
  const deadline = Date.now() + 4_000 * SLOW_BUILD_FACTOR;
  sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
  let dump = "";
  while (Date.now() < deadline) {
    dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
    if (/active=1/.test(dump)) {
      break;
    }
    await sleep(25);
  }
  if (!/active=1/.test(dump)) {
    throw new Error(`create-annot-enters-edit-mode: keyboard selection did not start\n${dump}`);
  }
  await postChar(frame, "v");
  while (Date.now() < deadline) {
    dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
    if (/visual=1/.test(dump)) {
      break;
    }
    await sleep(25);
  }
  if (!/visual=1/.test(dump)) {
    throw new Error(`create-annot-enters-edit-mode: visual mode did not start\n${dump}`);
  }
  postMessage(frame, WM_KEYDOWN, VK_END, 0);
  postMessage(frame, WM_KEYUP, VK_END, 0);
  await sleep(200);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("create-annot-enters-edit-mode");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
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

    let s = await state(client);
    if (s.editToolbar) {
      throw new Error(`create-annot-enters-edit-mode: Edit PDF mode is on before anything was created\n${s.raw}`);
    }

    await selectLineWithKeyboard(client, frame);
    sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));
    s = await waitForState(client, (st) => st.annotations === 1, "highlight from a selection was not created");
    if (s.editToolbar) {
      throw new Error(
        `create-annot-enters-edit-mode: CmdCreateAnnotHighlight without openedit turned on Edit PDF\n${s.raw}`,
      );
    }

    await selectLineWithKeyboard(client, frame);
    const withEdit = await client.request(ControlCommand.TestInvokeCommand, ["CmdCreateAnnotHighlight openedit"]);
    if (withEdit[0] !== 0) {
      throw new Error(`create-annot-enters-edit-mode: CmdCreateAnnotHighlight openedit: ${String(withEdit[1] ?? "")}`);
    }
    s = await waitForState(
      client,
      (st) => st.editToolbar && st.annotations === 2,
      "CmdCreateAnnotHighlight openedit did not turn on Edit PDF mode",
    );

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await waitForState(client, (st) => !st.editToolbar, "toggling Edit PDF off did not take");

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    s = await waitForState(client, (st) => st.annotations === 3, "text annotation was not created");
    if (s.editToolbar) {
      throw new Error(
        `create-annot-enters-edit-mode: CmdCreateAnnotText without openedit turned on Edit PDF\n${s.raw}`,
      );
    }

    const textEdit = await client.request(ControlCommand.TestInvokeCommand, ["CmdCreateAnnotText openedit", 180, 340]);
    if (textEdit[0] !== 0) {
      throw new Error(`create-annot-enters-edit-mode: CmdCreateAnnotText openedit: ${String(textEdit[1] ?? "")}`);
    }
    s = await waitForState(
      client,
      (st) => st.editToolbar && st.annotations === 4,
      "CmdCreateAnnotText openedit did not turn on Edit PDF mode",
    );

    sendMessage(frame, WM_COMMAND, cmdId("CmdCopyAnnotation"), packCoords(180, 340));
    await sleep(200);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await waitForState(client, (st) => !st.editToolbar, "toggling Edit PDF off before the paste did not take");
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteAnnotation"), packCoords(300, 400));
    s = await waitForState(
      client,
      (st) => st.editToolbar && st.annotations === 5,
      "pasting an annotation did not turn on Edit PDF mode",
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("create-annot-enters-edit-mode: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
