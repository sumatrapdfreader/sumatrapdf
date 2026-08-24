// Command Palette filtering includes the effective shortcut shown on the right,
// and matching shortcut text gets the same highlight as command descriptions.
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

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

type PaletteState = { items: number; queryLen: number; selectedCmdId: number };

async function paletteState(client: ControlClient): Promise<PaletteState | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`command-palette-shortcut-filter: TestCommandPalette failed: ${out}`);
  }
  const m = /OK sel=-?\d+ items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+) cmd=(-?\d+)/.exec(out);
  if (!m) {
    throw new Error(`command-palette-shortcut-filter: could not parse: ${out}`);
  }
  return { items: +m[1]!, queryLen: +m[2]!, selectedCmdId: +m[3]! };
}

function findPalette(frame: number): { palette: number; edit: number } {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

function countYellowInRightHalf(palette: number): number {
  const shot = captureWindowPixels(palette);
  if (!shot) {
    return 0;
  }
  let count = 0;
  for (let y = 0; y < shot.h; y++) {
    for (let x = Math.floor(shot.w / 2); x < shot.w; x++) {
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
  const appdata = tmpPath("command-palette-shortcut-filter");
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
      throw new Error("command-palette-shortcut-filter: palette did not open");
    }

    const query = ">F2";
    sendText(handles.edit, query);
    const filterDeadline = Date.now() + 3_000;
    let state: PaletteState | null = null;
    while (Date.now() < filterDeadline) {
      state = await paletteState(client);
      if (state && state.queryLen === query.length) {
        break;
      }
      await sleep(40);
    }
    const expected = cmdId("CmdRenameFile");
    if (!state || state.items !== 1 || state.selectedCmdId !== expected) {
      throw new Error(`command-palette-shortcut-filter: shortcut query failed: ${JSON.stringify(state)}`);
    }

    const paintDeadline = Date.now() + 3_000;
    let yellow = 0;
    while (Date.now() < paintDeadline) {
      yellow = countYellowInRightHalf(handles.palette);
      if (yellow > 20) {
        break;
      }
      await sleep(40);
    }
    if (yellow <= 20) {
      captureWindowToPng(handles.palette, tmpPath("command-palette-shortcut-filter.png"));
      throw new Error(`command-palette-shortcut-filter: shortcut highlight not painted; yellow=${yellow}`);
    }

    console.log("command-palette-shortcut-filter: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
