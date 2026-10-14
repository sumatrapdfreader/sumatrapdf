// A command can be listed in the command palette under more than one name:
// "Browse Files In Folder..." finds CmdNavigateFilesInFolder as well as its
// own "Navigate Files in Folder..." does.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep, VK_ESCAPE } from "./winapi.ts";
import {
  killAndWait,
  launchControlled,
  paletteSetText,
  pressKey,
  sendCommand,
  waitForPalette,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

async function openPalette(client: ControlClient, frame: number): Promise<{ palette: number; edit: number }> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const deadline = Date.now() + 8_000;
  while (Date.now() < deadline) {
    if (IS_MAC) {
      if ((await paletteState(client)).open) {
        return { palette: frame, edit: frame };
      }
    } else {
      const handles = await waitForPalette(client, frame, 50);
      if (handles.edit) {
        return handles;
      }
    }
    await sleep(50);
  }
  throw new Error("command-palette-alt-names: palette did not open");
}

// types query into a freshly opened palette and returns the selected command
async function queryPalette(client: ControlClient, frame: number, query: string): Promise<PaletteState> {
  const handles = await openPalette(client, frame);
  await paletteSetText(client, handles, query);
  const deadline = Date.now() + 3_000;
  let state: PaletteState | null = null;
  while (Date.now() < deadline) {
    state = await paletteState(client);
    if (state?.open && state.queryLen === query.length) {
      break;
    }
    await sleep(40);
  }
  if (!state?.open) {
    throw new Error(`command-palette-alt-names: no palette state for '${query}'`);
  }
  await pressKey(handles.edit, VK_ESCAPE, 0);
  const closeDeadline = Date.now() + 3_000;
  while ((await paletteState(client)).open) {
    if (Date.now() > closeDeadline) {
      throw new Error(`command-palette-alt-names: palette did not close after '${query}'`);
    }
    await sleep(10);
  }
  return state;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("command-palette-alt-names");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const cases: [string, string[]][] = [
      ["CmdNavigateFilesInFolder", ["Browse Files In Folder...", "Navigate Files in Folder..."]],
      ["CmdAdvancedSettings", ["Advanced Options...", "Advanced Settings..."]],
    ];
    for (const [cmd, texts] of cases) {
      const expected = cmdId(cmd);
      for (const text of texts) {
        const state = await queryPalette(client, frame, ">" + text);
        if (state.cmd !== expected) {
          throw new Error(`command-palette-alt-names: '${text}' selected ${state.cmd}, want ${expected}`);
        }
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
