// #6111: creating a highlight in fullscreen must still show the property row
// so Contents can be edited. The row is a WS_POPUP; in fullscreen it must be
// HWND_TOPMOST or it sits behind the caption-less frame.
//
// Run: bun tests/issue-6111.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { findTopWindow, postMessage, sleep, VK_END, WM_CHAR, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";
import { findChildByClass, killAndWait, launchControlled, sendCommandSync, typeIntoInput } from "./win-automation.ts";

const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const NOTE = "fs-annot-note";

async function markupDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6111: could not read toolbar state\n${raw}`);
  }
  return raw;
}

async function waitForToolbar(client: ControlClient, what: string): Promise<string> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let raw = "";
  for (;;) {
    raw = await markupDump(client);
    if (/annotEditToolbar visible=1/.test(raw)) {
      const m = /annotEditToolbar .*/.exec(raw);
      return m ? m[0]! : raw;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6111: ${what}\n${raw}`);
    }
    await sleep(40);
  }
}

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
    throw new Error(`issue-6111: keyboard selection did not start\n${dump}`);
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
    throw new Error(`issue-6111: visual mode did not start\n${dump}`);
  }
  postMessage(frame, WM_KEYDOWN, VK_END, 0);
  postMessage(frame, WM_KEYUP, VK_END, 0);
  await sleep(200);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6111");
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

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    await client.waitForRenderIdle();
    await sleep(200);

    await selectLineWithKeyboard(client, frame);
    sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));
    await client.waitForRenderIdle();

    let dump = await waitForToolbar(client, "property row not shown after highlight in fullscreen");
    const created = /annotations=(\d+)/.exec(await markupDump(client));
    if (!created || +created[1]! < 1) {
      throw new Error(`issue-6111: highlight was not created in fullscreen\n${await markupDump(client)}`);
    }

    const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    while (!/ editing=1/.test(dump) && Date.now() < deadline) {
      await sleep(40);
      dump = await waitForToolbar(client, "property row disappeared in fullscreen");
    }
    if (!/ editing=1/.test(dump)) {
      throw new Error(`issue-6111: contents editor did not open in fullscreen: ${dump}\n${await markupDump(client)}`);
    }

    const tbHwnd = findTopWindow(proc.pid!, TOOLBAR_CLASS);
    if (!tbHwnd) {
      throw new Error("issue-6111: property row window not found");
    }

    const edit = findChildByClass(tbHwnd, "Edit");
    if (!edit) {
      throw new Error("issue-6111: contents edit box not found");
    }
    await typeIntoInput(edit, NOTE, false);
    await sleep(200);
    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6111: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
