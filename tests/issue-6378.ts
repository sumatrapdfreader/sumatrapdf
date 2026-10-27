// #6378: Ctrl+Backspace inserted 0x7F (a box) instead of deleting the previous
// word in the find box (an editable combo box) and the free text in-place editor.
// Ctrl+Backspace reaches an edit control as WM_CHAR 0x7F, so that is what we send.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { cmdId, makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  WM_CHAR,
  WM_COMMAND,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const EM_SETSEL = 0x00b1;
const kCtrlBackChar = 0x7f;

// set text, put the caret at the end, press Ctrl+Backspace; returns the new text
function ctrlBack(edit: number, text: string): string {
  sendText(edit, text);
  sendMessage(edit, EM_SETSEL, text.length, text.length);
  sendMessage(edit, WM_CHAR, kCtrlBackChar, 0);
  return getControlText(edit);
}

// DEL prints as nothing
function showDel(s: string): string {
  return s.replaceAll(String.fromCharCode(kCtrlBackChar), "\\x7f");
}

function check(what: string, edit: number, text: string, want: string): void {
  const got = ctrlBack(edit, text);
  if (got !== want) {
    throw new Error(`issue-6378: ${what}: Ctrl+Backspace on "${text}" gave "${showDel(got)}", want "${want}"`);
  }
}

// the find bar is its own top-level window; its edit is a combo box's child
function findBarEdit(pid: number, frame: number): number {
  let found = 0;
  enumWindows((top) => {
    if (top === frame || getWindowPid(top) !== pid) {
      return true;
    }
    enumChildWindows(top, (combo) => {
      if (getClassName(combo) !== "ComboBox") {
        return true;
      }
      enumChildWindows(combo, (h) => {
        if (getClassName(h) === "Edit") {
          found = h;
        }
        return found === 0;
      });
      return found === 0;
    });
    return found === 0;
  });
  return found;
}

async function waitFor(what: string, fn: () => number): Promise<number> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const h = fn();
    if (h) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6378: ${what} not found`);
    }
    await sleep(40);
  }
}

function inPlaceEdit(canvas: number): number {
  let found = 0;
  enumChildWindows(canvas, (h) => {
    if (getClassName(h) === "Edit" && getControlText(h).startsWith("This is a text")) {
      found = h;
    }
    return found === 0;
  });
  return found;
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP issue-6378: Win32 edit controls only");
    return;
  }
  const dir = tmpPath("issue-6378");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\nSearchUIFloating = false\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdFindFirst"));
    const findEdit = await waitFor("find box edit", () => findBarEdit(proc.pid!, frame));
    check("find box", findEdit, "foo bar", "foo ");
    check("find box", findEdit, "foo bar  ", "foo ");
    check("find box", findEdit, "one", "");

    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(120, 250));
    const box = await waitFor("free text in-place edit", () => inPlaceEdit(canvas));
    check("free text", box, "foo bar", "foo ");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6378: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
