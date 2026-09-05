// #6137: the Contents edit must survive a WM_KILLFOCUS with no new focus
// (what the on-screen keyboard does on a tablet). Clicking the page still
// commits, same as annot-contents-click-away.
//
// Run: bun tests/issue-6137-contents.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { findTopWindow, packCoords, sendMessage, sleep, WM_COMMAND, WM_KILLFOCUS } from "./winapi.ts";
import {
  clickAt,
  findCanvas,
  findChildByClass,
  killAndWait,
  launchControlled,
  sendCommand,
  typeIntoInput,
} from "./win-automation.ts";

const TEXT = "kept-through-osk-focus";
const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";

type Rect = { x: number; y: number; dx: number; dy: number };

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

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
    throw new Error(`issue-6137-contents: could not read toolbar state\n${raw}`);
  }
  return m[0]!;
}

async function selectedContents(client: ControlClient): Promise<string | null> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-6137-contents: could not read annotation state: ${raw}`);
  }
  const m = / contents=(.*)$/.exec(raw);
  return m ? m[1]! : null;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6137-contents");
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
    const canvas = findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    await sleep(400);
    await client.waitForRenderIdle();

    let dump = await toolbarDump(client);
    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const contentsChip = parseRect(/[=;]contents:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    if (contentsChip.dx === 0) {
      throw new Error(`issue-6137-contents: no contents chip: ${dump}`);
    }
    const tbHwnd = findTopWindow(proc.pid!, TOOLBAR_CLASS);
    if (!tbHwnd) {
      throw new Error("issue-6137-contents: property row window not found");
    }

    await clickAt(
      tbHwnd,
      contentsChip.x - placed.x + Math.floor(contentsChip.dx / 2),
      contentsChip.y - placed.y + Math.floor(contentsChip.dy / 2),
    );
    await sleep(400);
    dump = await toolbarDump(client);
    if (!/ editing=1/.test(dump)) {
      throw new Error(`issue-6137-contents: contents editor did not open: ${dump}`);
    }

    const edit = findChildByClass(tbHwnd, "Edit");
    if (!edit) {
      throw new Error("issue-6137-contents: contents edit box not found");
    }
    await typeIntoInput(edit, TEXT, false);
    await sleep(200);

    // OSK: kill-focus with no window taking it. Must not close the box.
    sendMessage(edit, WM_KILLFOCUS, 0, 0);
    await sleep(400);
    dump = await toolbarDump(client);
    if (!/ editing=1/.test(dump)) {
      throw new Error(`issue-6137-contents: OSK kill-focus closed the editor: ${dump}`);
    }

    await clickAt(canvas, 420, 620);
    await sleep(600);
    await client.waitForRenderIdle();

    dump = await toolbarDump(client);
    if (!/annotEditToolbar visible=1/.test(dump) || !/ editing=0/.test(dump)) {
      throw new Error(`issue-6137-contents: click-away did not commit: ${dump}`);
    }
    const contents = await selectedContents(client);
    if (contents !== TEXT) {
      throw new Error(`issue-6137-contents: contents are "${contents}", want "${TEXT}"`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
