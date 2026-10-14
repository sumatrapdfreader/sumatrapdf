// Command Palette filtering includes the effective shortcut shown on the right,
// and matching shortcut text gets the same highlight as command descriptions.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, captureWindowToPng, sleep } from "./winapi.ts";
import {
  killAndWait,
  launchControlled,
  sendCommand,
  waitForPalette,
  paletteSetText,
  isFilterMatchTint,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

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
      if (isFilterMatchTint(r, g, b)) {
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
      if (IS_MAC) {
        if ((await paletteState(client)).open) {
          handles = { palette: 1, edit: 1 };
          break;
        }
      } else {
        handles = await waitForPalette(client, frame, 50);
        if (handles.palette && handles.edit) {
          break;
        }
      }
      await sleep(50);
    }
    if (!handles.palette || !handles.edit) {
      throw new Error("command-palette-shortcut-filter: palette did not open");
    }

    const query = ">F2";
    await paletteSetText(client, handles, query);
    const filterDeadline = Date.now() + 3_000;
    let state: PaletteState | null = null;
    while (Date.now() < filterDeadline) {
      state = await paletteState(client);
      if (state?.open && state.queryLen === query.length) {
        break;
      }
      await sleep(40);
    }
    const expected = cmdId("CmdRenameFile");
    if (!state?.open || state.items !== 1 || state.cmd !== expected) {
      throw new Error(`command-palette-shortcut-filter: shortcut query failed: ${JSON.stringify(state)}`);
    }

    if (IS_MAC) {
      console.log("SKIP command-palette-shortcut-filter highlight: macOS has no palette window to sample");
    } else {
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
