// Image editing commands dispatched by the command palette operate on the
// current standalone image. Embedded PDF images use the canvas context menu,
// which retains the image under the cursor after the menu closes.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, SLOW_BUILD_FACTOR, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import {
  findPalette,
  killAndWait,
  launchControlled,
  paletteSetText,
  sendCommand,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

async function waitForItems(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 8_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const state = await paletteState(client);
    if (state.open && state.items > 1) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error("image-only-palette-items: command palette did not open");
    }
    await sleep(50);
  }
}

async function querySelectedCommand(client: ControlClient, frame: number, query: string): Promise<number> {
  const text = `>${query}`;
  const deadline = Date.now() + 3_000 * SLOW_BUILD_FACTOR;
  let lastState: PaletteState | null = null;
  let sent = false;
  for (;;) {
    let state = await paletteState(client);
    if (!state.open) {
      sendCommand(frame, cmdId("CmdCommandPalette"));
      await waitForItems(client);
      sent = false;
      state = await paletteState(client);
    }
    // mac has no palette edit HWND; TestInput sets the query.
    const handles = IS_MAC ? { palette: 1, edit: 1 } : findPalette(frame);
    if (state.open && handles.edit && !sent) {
      await paletteSetText(client, handles, text);
      sent = true;
      state = await paletteState(client);
    }
    lastState = state;
    if (state.open && state.queryLen === text.length) {
      return state.items > 0 ? state.cmd : 0;
    }
    if (Date.now() > deadline) {
      throw new Error(`image-only-palette-items: query did not update: ${query}; state=${JSON.stringify(lastState)}`);
    }
    await sleep(40);
  }
}

const imageCommands = [
  ["Save Image", "CmdSaveImage"],
  ["Crop Image", "CmdCropImage"],
  ["Resize Image", "CmdResizeImage"],
  ["Convert Image To PDF", "CmdConvertImageToPdf"],
] as const;

async function checkDocument(file: string, shouldShow: boolean, appData: string): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appData, file]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));
    await waitForItems(client);
    if (!IS_MAC && !findPalette(frame).edit) {
      throw new Error("image-only-palette-items: no command palette edit");
    }

    for (const [query, commandName] of imageCommands) {
      const expected = cmdId(commandName);
      const selected = await querySelectedCommand(client, frame, query);
      if ((selected === expected) !== shouldShow) {
        throw new Error(
          `image-only-palette-items: ${commandName} selected=${selected}, expected ${shouldShow ? expected : "hidden"}`,
        );
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("image-only-palette-items");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  await checkDocument(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), false, dir);
  await checkDocument(join(ROOT, "tests", "issue-1201-data", "001.png"), true, dir);

  console.log("image-only-palette-items: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
