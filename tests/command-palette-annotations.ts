// Command Palette `*` lists PDF annotations (same rows/filter as Find Annotation)
// and Enter jumps to the selected one.
//
// Run: bun tests/command-palette-annotations.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  getClassName,
  getFocusedHwnd,
  getRootWindow,
  postMessage,
  sendText,
  sleep,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

type PaletteState = {
  items: number;
  queryLen: number;
  annots: number;
  annotPage: number;
  annotsDone: number;
};

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /Contents (palette-annot-alpha) >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 500 220 530] " +
      "/QuadPoints [72 530 220 530 72 500 220 500] /C [1 1 0] /Contents (palette-annot-beta) >>",
  ];
  return assemblePdf(objs);
}

function findPalette(frame: number): { palette: number; edit: number } {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

async function paletteState(client: ControlClient): Promise<PaletteState | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`command-palette-annotations: TestCommandPalette failed: ${out.trim()}`);
  }
  const m =
    /OK sel=-?\d+ items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+) cmd=-?\d+ rtl=\d+ thumb=\d+ page=\d+ rendered=\d+ annots=(\d+) annotPage=(\d+) annotsDone=(\d+)/.exec(
      out,
    );
  if (!m) {
    throw new Error(`command-palette-annotations: could not parse: ${out.trim()}`);
  }
  return { items: +m[1]!, queryLen: +m[2]!, annots: +m[3]!, annotPage: +m[4]!, annotsDone: +m[5]! };
}

async function annotContents(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, []);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("command-palette-annotations");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));

    const openDeadline = Date.now() + 8_000;
    let handles = { palette: 0, edit: 0 };
    while (Date.now() < openDeadline) {
      handles = findPalette(frame);
      if (handles.palette && handles.edit) {
        break;
      }
      await sleep(50);
    }
    if (!handles.palette || !handles.edit) {
      throw new Error("command-palette-annotations: palette did not open");
    }

    const annotDeadline = Date.now() + 8_000;
    let collected: PaletteState | null = null;
    while (Date.now() < annotDeadline) {
      collected = await paletteState(client);
      if (collected && collected.annots >= 2) {
        break;
      }
      await sleep(40);
    }
    if (!collected || collected.annots < 2) {
      throw new Error(`command-palette-annotations: annotations not collected: ${JSON.stringify(collected)}`);
    }

    const query = "* palette-annot-beta";
    sendText(handles.edit, query);
    const filterDeadline = Date.now() + 3_000;
    let filtered: PaletteState | null = null;
    while (Date.now() < filterDeadline) {
      filtered = await paletteState(client);
      if (filtered && filtered.queryLen === query.length && filtered.items === 1) {
        break;
      }
      await sleep(40);
    }
    if (!filtered || filtered.items !== 1) {
      throw new Error(`command-palette-annotations: filter failed: ${JSON.stringify(filtered)}`);
    }

    postMessage(handles.palette, WM_KEYDOWN, VK_RETURN, 0);
    const closeDeadline = Date.now() + 3_000;
    while (Date.now() < closeDeadline) {
      if ((await paletteState(client)) === null) {
        break;
      }
      await sleep(40);
    }
    if ((await paletteState(client)) !== null) {
      throw new Error("command-palette-annotations: palette did not close");
    }

    const jumpDeadline = Date.now() + 3_000;
    let dump = "";
    while (Date.now() < jumpDeadline) {
      dump = await annotContents(client);
      if (dump.includes("palette-annot-beta")) {
        break;
      }
      await sleep(40);
    }
    if (!dump.includes("palette-annot-beta")) {
      throw new Error(`command-palette-annotations: did not jump to annotation: ${dump.trim()}`);
    }

    console.log("command-palette-annotations: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
