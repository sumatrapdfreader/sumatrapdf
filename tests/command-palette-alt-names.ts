// A command can be listed in the command palette under more than one name:
// "Browse Files In Folder..." finds CmdNavigateFilesInFolder as well as its
// own "Navigate Files in Folder..." does.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { getClassName, getFocusedHwnd, getRootWindow, sendText, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, pressEscape, sendCommand } from "./win-automation.ts";

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
    throw new Error(`command-palette-alt-names: TestCommandPalette failed: ${out}`);
  }
  const m = /OK sel=-?\d+ items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+) cmd=(-?\d+)/.exec(out);
  if (!m) {
    throw new Error(`command-palette-alt-names: could not parse: ${out}`);
  }
  return { items: +m[1]!, queryLen: +m[2]!, selectedCmdId: +m[3]! };
}

async function openPalette(frame: number): Promise<number> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const deadline = Date.now() + 8_000;
  while (Date.now() < deadline) {
    const edit = getFocusedHwnd(frame);
    if (edit && getClassName(edit) === "Edit" && getRootWindow(edit) !== frame) {
      return edit;
    }
    await sleep(50);
  }
  throw new Error("command-palette-alt-names: palette did not open");
}

// types query into a freshly opened palette and returns the selected command
async function queryPalette(client: ControlClient, frame: number, query: string): Promise<PaletteState> {
  const edit = await openPalette(frame);
  sendText(edit, query);
  const deadline = Date.now() + 3_000;
  let state: PaletteState | null = null;
  while (Date.now() < deadline) {
    state = await paletteState(client);
    if (state && state.queryLen === query.length) {
      break;
    }
    await sleep(40);
  }
  if (!state) {
    throw new Error(`command-palette-alt-names: no palette state for '${query}'`);
  }
  await pressEscape(edit);
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
        if (state.selectedCmdId !== expected) {
          throw new Error(`command-palette-alt-names: '${text}' selected ${state.selectedCmdId}, want ${expected}`);
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
