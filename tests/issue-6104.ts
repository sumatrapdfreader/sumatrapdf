// #6104: Command Palette File History must keep the filename visible when the
// directory path is very long. The directory used to take its full measured
// width, leaving no room for the name.
//
// Seeds FileHistory with a long path, filters the palette to that basename, and
// checks the filter highlight is painted on the left (the name), not squeezed
// off by the directory.
//
// Run: bun tests/issue-6104.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  captureWindowPixels,
  captureWindowToPng,
  getClassName,
  getFocusedHwnd,
  getRootWindow,
  sendText,
  sleep,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const FILE_NAME = "issue6104.pdf";
const LONG_DIR = ["C:", ...Array(30).fill("zzzzzzzzzzzzzzzzzzzz")].join("\\");
const LONG_PATH = `${LONG_DIR}\\${FILE_NAME}`;

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
FileStates [
	[
		FilePath = ${LONG_PATH}
		OpenCount = 3
	]
]
`;

type PaletteState = { items: number; queryLen: number };

async function paletteState(client: ControlClient): Promise<PaletteState | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`issue-6104: TestCommandPalette failed: ${out}`);
  }
  const m = /OK sel=-?\d+ items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+)/.exec(out);
  if (!m) {
    throw new Error(`issue-6104: could not parse: ${out}`);
  }
  return { items: +m[1]!, queryLen: +m[2]! };
}

function findPalette(frame: number): { palette: number; edit: number } {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

function countYellowInLeftHalf(palette: number): number {
  const shot = captureWindowPixels(palette);
  if (!shot) {
    return 0;
  }
  let count = 0;
  const xMax = Math.floor(shot.w / 2);
  for (let y = 0; y < shot.h; y++) {
    for (let x = 0; x < xMax; x++) {
      const off = (y * shot.w + x) * 4;
      const b = shot.data[off]!;
      const g = shot.data[off + 1]!;
      const r = shot.data[off + 2]!;
      if (r > 240 && g > 240 && b < 30) {
        count++;
      }
    }
  }
  return count;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6104");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
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
      throw new Error("issue-6104: palette did not open");
    }

    const query = `#${FILE_NAME}`;
    sendText(handles.edit, query);
    const filterDeadline = Date.now() + 3_000;
    let state: PaletteState | null = null;
    while (Date.now() < filterDeadline) {
      state = await paletteState(client);
      if (state && state.queryLen === query.length && state.items === 1) {
        break;
      }
      await sleep(40);
    }
    if (!state || state.items !== 1) {
      throw new Error(`issue-6104: file history query failed: ${JSON.stringify(state)}`);
    }

    const paintDeadline = Date.now() + 3_000;
    let yellow = 0;
    while (Date.now() < paintDeadline) {
      yellow = countYellowInLeftHalf(handles.palette);
      if (yellow > 20) {
        break;
      }
      await sleep(40);
    }
    if (yellow <= 20) {
      captureWindowToPng(handles.palette, tmpPath("issue-6104.png"));
      throw new Error(`issue-6104: filename highlight not painted on the left; yellow=${yellow}`);
    }

    console.log("issue-6104: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
