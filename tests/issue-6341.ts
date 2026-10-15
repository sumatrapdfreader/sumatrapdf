// Issue #6341: reopening Edit note on a long annotation sized the card to the
// note laid out on one line, and the card has no resize border.
//
// Run: bun tests/issue-6341.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findTopWindow, getControlText, packCoords, sendMessage, sendText, WM_COMMAND } from "./winapi.ts";
import {
  clickAt,
  findCanvas,
  findChildByClass,
  killAndWait,
  launchControlled,
  sendCommandSync,
} from "./win-automation.ts";

const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const NOTE = "The reviewer asked for a shorter sentence here. ".repeat(8).trimEnd();

type Rect = { x: number; y: number; dx: number; dy: number };

function parseRect(m: RegExpExecArray | null): Rect {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /annotEditToolbar .*/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6341: could not read toolbar state\n${raw}`);
  }
  return m[0]!;
}

async function selectedContents(client: ControlClient): Promise<string | null> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6341: could not read annotation state: ${raw}`);
  }
  const m = / contents=(.*)$/.exec(raw);
  return m ? m[1]! : null;
}

const chipRe = /[=;]contents:(-?\d+),(-?\d+),(\d+),(\d+)/;

async function clickContents(client: ControlClient, pid: number): Promise<void> {
  const dump = await pollUntil(
    async () => toolbarDump(client),
    (s) => parseRect(chipRe.exec(s)).dx > 0,
    {
      error: (s) => `issue-6341: no contents chip: ${s}`,
    },
  );
  const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
  const chip = parseRect(chipRe.exec(dump));
  if (USE_NG) {
    const res = await client.request(ControlCommand.TestInput, [
      "click",
      chip.x + Math.floor(chip.dx / 2),
      chip.y + Math.floor(chip.dy / 2),
      0,
      0,
    ]);
    if (res[0] !== 0 || !String(res[1] ?? "").startsWith("OK")) {
      throw new Error(`issue-6341: contents click failed: ${String(res[1] ?? "")}`);
    }
    return;
  }
  const hwnd = findTopWindow(pid, TOOLBAR_CLASS);
  if (!hwnd) {
    throw new Error("issue-6341: property row window not found");
  }
  await clickAt(hwnd, chip.x - placed.x + Math.floor(chip.dx / 2), chip.y - placed.y + Math.floor(chip.dy / 2), 0);
}

async function editorWidth(client: ControlClient): Promise<number> {
  const dump = await pollUntil(
    async () => toolbarDump(client),
    (s) => / editing=1/.test(s),
    {
      error: (s) => `issue-6341: contents editor did not open: ${s}`,
    },
  );
  return parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump)).dx;
}

async function setNote(client: ControlClient, pid: number): Promise<void> {
  if (USE_NG) {
    for (const ch of NOTE) {
      await client.request(ControlCommand.TestInput, ["char", ch.charCodeAt(0), 0, 0, 0]);
    }
    await pollUntil(
      async () => String((await client.request(ControlCommand.TestUiState, []))[1] ?? ""),
      (s) => s.includes(NOTE),
      { error: (s) => `issue-6341: edit box holds no note\n${s}` },
    );
    return;
  }
  const hwnd = findTopWindow(pid, TOOLBAR_CLASS);
  const edit = hwnd ? findChildByClass(hwnd, "Edit") : 0;
  if (!edit) {
    throw new Error("issue-6341: contents edit box not found");
  }
  sendText(edit, NOTE);
  const got = getControlText(edit);
  if (!got.includes(NOTE)) {
    throw new Error(`issue-6341: edit box holds "${got.slice(0, 40)}", want the note`);
  }
}

async function acceptNote(client: ControlClient, frame: number): Promise<void> {
  if (USE_NG) {
    const box = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await toolbarDump(client)));
    const x = box.x > 80 ? 24 : box.x + box.dx + 24;
    const y = box.y + Math.floor(box.dy / 2);
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
  } else {
    await clickAt(findCanvas(frame), 420, 620, 0);
  }
  await pollUntil(
    async () => toolbarDump(client),
    (s) => / editing=0/.test(s),
    {
      error: (s) => `issue-6341: accepting the note did not close the editor: ${s}`,
    },
  );
  const contents = await selectedContents(client);
  if (contents !== NOTE) {
    throw new Error(`issue-6341: saved note len ${contents?.length ?? -1} want ${NOTE.length}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6341");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

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
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    await client.waitForRenderIdle();

    await clickContents(client, proc.pid!);
    const before = await editorWidth(client);
    if (before < 80) {
      throw new Error(`issue-6341: empty editor is ${before}px wide`);
    }

    await setNote(client, proc.pid!);
    await acceptNote(client, frame);
    await clickContents(client, proc.pid!);
    const after = await editorWidth(client);
    // a one-line note is thousands of pixels; the empty editor's width is the useful one
    if (after > before + 48) {
      throw new Error(`issue-6341: reopening the note widened the editor from ${before} to ${after}`);
    }
    console.log("issue-6341: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
