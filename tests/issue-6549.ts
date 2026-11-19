// #6549: a free text edited after an undo showed garbage glyphs. Undo took away
// the font object its first appearance had added, but MuPDF's font resource
// cache still pointed at it, so the next appearance referenced a missing font.
// Create and type a free text, undo it all, do it again, save, and check that
// every font the saved appearances use exists.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { ControlCommand } from "./control.ts";
import { cmdId, makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  enumChildWindows,
  getClassName,
  getControlText,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  WM_CHAR,
  WM_COMMAND,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

type State = { raw: string; annotations: number; editActive: boolean; modified: boolean };

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const count = /annotations=(\d+)/.exec(raw);
  const edit = /freeTextEdit active=(\d)/.exec(raw);
  const undo = /undo canUndo=\d canRedo=\d modified=(\d)/.exec(raw);
  return {
    raw,
    annotations: count ? +count[1]! : -1,
    editActive: edit?.[1] === "1",
    modified: undo?.[1] === "1",
  };
}

async function waitFor(client: ControlClient, pred: (s: State) => boolean, what: string): Promise<void> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await state(client);
    if (pred(s)) return;
    if (Date.now() > deadline) throw new Error(`issue-6549: ${what}\n${s.raw}`);
    await sleep(50);
  }
}

function findEditBox(canvas: number): number {
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (getClassName(hwnd) !== "Edit" || !getControlText(hwnd).startsWith("This is a text")) return true;
    found = hwnd;
    return false;
  });
  return found;
}

async function typeFreeText(client: ControlClient, frame: number): Promise<void> {
  sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(120, 250));
  await waitFor(client, (s) => s.editActive, "creating a free text did not open the in-place editor");
  // ng edits in a gpui text area on the frame, orig in an Edit child of the canvas
  const box = USE_NG ? frame : findEditBox(findCanvas(frame));
  if (!box) throw new Error("issue-6549: no in-place edit box");
  sendText(box, "asas101");
  // Ctrl+Enter reaches the edit control as LF
  sendMessage(box, WM_CHAR, 0x0a, 0);
  await waitFor(client, (s) => !s.editActive && s.annotations === 1, "the edit did not commit");
}

// "/Helv 7 0 R" in a /Font resource dict => each referenced object must be a font
function missingFonts(pdf: string): string[] {
  const missing: string[] = [];
  for (const m of pdf.matchAll(/\/Font\s*<<([^>]*)>>/g)) {
    for (const ref of m[1]!.matchAll(/\/(\w+)\s+(\d+)\s+0\s+R/g)) {
      const defs = [...pdf.matchAll(new RegExp(`(?:^|\\s)${ref[2]} 0 obj\\s*<<([\\s\\S]*?)endobj`, "g"))];
      const last = defs.at(-1)?.[1] ?? "";
      if (!/\/Type\s*\/Font/.test(last)) missing.push(`${ref[1]} -> ${ref[2]} 0 R`);
    }
  }
  return missing;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6549");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    await typeFreeText(client, frame);
    // take back the text and the annotation, and with them the font object
    for (let i = 0; i < 2; i++) {
      sendCommandSync(frame, cmdId("CmdUndo"));
      await client.waitForRenderIdle();
    }
    await waitFor(client, (s) => s.annotations === 0, "two undos did not remove the free text");
    await typeFreeText(client, frame);

    sendCommand(frame, cmdId("CmdSaveAnnotations"));
    await waitFor(client, (s) => !s.modified, "saving did not finish");
    const saved = readFileSync(pdf, "latin1");
    if (!/\/FreeText/.test(saved)) throw new Error("issue-6549: the saved PDF has no free text");
    const missing = missingFonts(saved);
    if (missing.length > 0) {
      throw new Error(`issue-6549: the free text appearance uses fonts that don't exist: ${missing.join(", ")}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
