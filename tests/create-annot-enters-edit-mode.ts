// Creating an annotation by any means turns on Edit PDF mode: that is where
// the new annotation can be selected, moved, resized and edited. Covers the
// three routes into the frame's create handler: from a text selection, from a
// canvas point (context menu / toolbar), and from a paste.

import { mkdirSync, writeFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import {
  packCoords,
  postMessage,
  sendMessage,
  sleep,
  VK_END,
  WM_CHAR,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_KEYUP,
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

async function waitForEditMode(client: ControlClient, want: boolean, what: string): Promise<State> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await state(client);
    if (s.editToolbar === want) {
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
  postMessage(frame, WM_CHAR, "v".charCodeAt(0), 0);
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
  // a PDF with text, so a selection-based annotation has something to mark up
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

    // from a text selection (the classic "a" shortcut route, which used to
    // need Shift to also enter Edit PDF mode)
    await selectLineWithKeyboard(client, frame);
    sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));
    s = await waitForEditMode(client, true, "a highlight from a selection did not turn on Edit PDF mode");
    if (s.annotations !== 1) {
      throw new Error(`create-annot-enters-edit-mode: expected one annotation, got ${s.annotations}\n${s.raw}`);
    }

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await waitForEditMode(client, false, "toggling Edit PDF off did not take");

    // from a canvas point (context menu / toolbar route)
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    s = await waitForEditMode(client, true, "a text annotation did not turn on Edit PDF mode");
    if (s.annotations !== 2) {
      throw new Error(`create-annot-enters-edit-mode: expected two annotations, got ${s.annotations}\n${s.raw}`);
    }

    // from a paste
    sendMessage(frame, WM_COMMAND, cmdId("CmdCopyAnnotation"), packCoords(150, 300));
    await sleep(200);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await waitForEditMode(client, false, "toggling Edit PDF off before the paste did not take");
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteAnnotation"), packCoords(300, 400));
    s = await waitForEditMode(client, true, "pasting an annotation did not turn on Edit PDF mode");
    if (s.annotations !== 3) {
      throw new Error(`create-annot-enters-edit-mode: expected three annotations, got ${s.annotations}\n${s.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("create-annot-enters-edit-mode: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
