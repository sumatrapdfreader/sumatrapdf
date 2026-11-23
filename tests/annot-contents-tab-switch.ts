// Text typed in the Contents edit box survives a tab switch: the box is
// committed before it goes away, as a click away would (round 5 review C14).

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, pollUntil, runStandalone, tmpPath, USE_NG, makeBlankPdf } from "./util.ts";
import { findTopWindow, getControlText, packCoords, sendMessage, WM_COMMAND } from "./winapi.ts";
import {
  clickAt,
  findChildByClass,
  killAndWait,
  launchControlled,
  sendCommandSync,
  typeIntoInput,
} from "./win-automation.ts";

const TEXT = "kept-on-tab-switch";
const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";

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
    throw new Error(`annot-contents-tab-switch: could not read toolbar state\n${raw}`);
  }
  return m[0]!;
}

// how many of the current document's annotations match `text`, through the
// annotation list's filter, which also searches the contents. The tab switch
// drops the selection, so the selected annotation can't be asked.
async function annotsMatching(client: ControlClient, text: string): Promise<number> {
  const res = await client.request(ControlCommand.TestAnnotFilter, ["set", text]);
  const raw = String(res[1] ?? "");
  const m = / nVisible=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`annot-contents-tab-switch: could not read the annotation list: ${raw}`);
  }
  return Number(m[1]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-contents-tab-switch");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const other = join(dir, "other.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(other, makeBlankPdf(), "latin1");
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
    other,
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));

    // a text (sticky note) annotation: its Contents chip opens the edit box on
    // the property row, not the free text in-place editor
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    await client.waitForRenderIdle();

    let dump = await toolbarDump(client);
    const chipRe = /[=;]contents:(-?\d+),(-?\d+),(\d+),(\d+)/;
    if (USE_NG && parseRect(chipRe.exec(dump)).dx === 0) {
      dump = await pollUntil(
        async () => toolbarDump(client),
        (s) => parseRect(chipRe.exec(s)).dx > 0,
        {
          error: (s) => `annot-contents-tab-switch: no contents chip: ${s}`,
        },
      );
    }
    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const contentsChip = parseRect(chipRe.exec(dump));
    if (contentsChip.dx === 0) {
      throw new Error(`annot-contents-tab-switch: no contents chip: ${dump}`);
    }

    let typed = "";
    if (USE_NG) {
      // the row is drawn in the frame. There is no edit HWND; the focused
      // text area takes the characters.
      const res = await client.request(ControlCommand.TestInput, [
        "click",
        contentsChip.x + Math.floor(contentsChip.dx / 2),
        contentsChip.y + Math.floor(contentsChip.dy / 2),
        0,
        0,
      ]);
      if (res[0] !== 0 || !String(res[1] ?? "").startsWith("OK")) {
        throw new Error(`annot-contents-tab-switch: contents click failed: ${String(res[1] ?? "")}`);
      }
      await pollUntil(
        async () => toolbarDump(client),
        (s) => / editing=1/.test(s),
        {
          error: (s) => `annot-contents-tab-switch: contents editor did not open: ${s}`,
        },
      );
      await pollUntil(
        async () => String((await client.request(ControlCommand.TestUiState, []))[1] ?? ""),
        (s) => /edit=1/.test(s),
        { error: "annot-contents-tab-switch: contents edit did not take focus" },
      );
      for (const ch of TEXT) {
        await client.request(ControlCommand.TestInput, ["char", ch.charCodeAt(0), 0, 0, 0]);
      }
      const state = await pollUntil(
        async () => String((await client.request(ControlCommand.TestUiState, []))[1] ?? ""),
        (s) => /editText='[^']*kept-on-tab-switch/.test(s),
        { error: (s) => `annot-contents-tab-switch: edit box holds "${s}", want "${TEXT}"` },
      );
      typed = /editText='([^']*)'/.exec(state)?.[1] ?? "";
    } else {
      const tbHwnd = findTopWindow(proc.pid!, TOOLBAR_CLASS);
      if (!tbHwnd) {
        throw new Error("annot-contents-tab-switch: property row window not found");
      }

      await clickAt(
        tbHwnd,
        contentsChip.x - placed.x + Math.floor(contentsChip.dx / 2),
        contentsChip.y - placed.y + Math.floor(contentsChip.dy / 2),
        0,
      );
      dump = await toolbarDump(client);
      if (!/ editing=1/.test(dump)) {
        throw new Error(`annot-contents-tab-switch: contents editor did not open: ${dump}`);
      }

      const edit = findChildByClass(tbHwnd, "Edit");
      if (!edit) {
        throw new Error("annot-contents-tab-switch: contents edit box not found");
      }
      await typeIntoInput(edit, TEXT, false);

      // a stray real keystroke can land in the focused box; the check is that
      // whatever it holds survives the click away
      typed = getControlText(edit);
    }
    if (!typed.includes(TEXT)) {
      throw new Error(`annot-contents-tab-switch: edit box holds "${typed}", want "${TEXT}"`);
    }

    // switch away while the box is open, then back
    sendCommandSync(frame, cmdId("CmdPrevTab"));
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdNextTab"));
    await client.waitForRenderIdle();

    dump = await toolbarDump(client);
    if (!/ editing=0/.test(dump)) {
      throw new Error(`annot-contents-tab-switch: editor still open: ${dump}`);
    }
    if ((await annotsMatching(client, TEXT)) !== 1) {
      throw new Error(`annot-contents-tab-switch: no annotation holds "${typed}" after the tab switch`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-contents-tab-switch: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
