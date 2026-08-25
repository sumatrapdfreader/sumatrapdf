// Image editing commands dispatched by the command palette operate on the
// current standalone image. Embedded PDF images use the canvas context menu,
// which retains the image under the cursor after the menu closes.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, SLOW_BUILD_FACTOR, cmdId, runStandalone, tmpPath } from "./util.ts";
import { enumWindows, findChildWindow, getWindowPid, isWindowVisible, sendText, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
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
    throw new Error(`image-only-palette-items: TestCommandPalette failed: ${out}`);
  }
  const m = /OK sel=-?\d+ items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+) cmd=(-?\d+)/.exec(out);
  if (!m) {
    throw new Error(`image-only-palette-items: could not parse: ${out}`);
  }
  return { items: +m[1]!, queryLen: +m[2]!, selectedCmdId: +m[3]! };
}

function findPaletteEdit(pid: number, frame: number): number {
  let edit = 0;
  enumWindows((hwnd) => {
    if (hwnd === frame || getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    edit = findChildWindow(hwnd, "Edit");
    return edit === 0;
  });
  return edit;
}

async function waitForPalette(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 8_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const state = await paletteState(client);
    if (state && state.items > 1) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error("image-only-palette-items: command palette did not open");
    }
    await sleep(50);
  }
}

async function querySelectedCommand(client: ControlClient, pid: number, frame: number, query: string): Promise<number> {
  const text = `>${query}`;
  const deadline = Date.now() + 3_000 * SLOW_BUILD_FACTOR;
  let lastState: PaletteState | null = null;
  let sent = false;
  for (;;) {
    let state = await paletteState(client);
    if (!state) {
      sendCommand(frame, cmdId("CmdCommandPalette"));
      await waitForPalette(client);
      sent = false;
      state = await paletteState(client);
    }
    const edit = findPaletteEdit(pid, frame);
    if (state && edit && !sent) {
      sendText(edit, text);
      sent = true;
      state = await paletteState(client);
    }
    lastState = state;
    if (state && state.queryLen === text.length) {
      return state.items > 0 ? state.selectedCmdId : 0;
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
    await waitForPalette(client);
    if (!findPaletteEdit(proc.pid!, frame)) {
      throw new Error("image-only-palette-items: no command palette edit");
    }

    for (const [query, commandName] of imageCommands) {
      const expected = cmdId(commandName);
      const selected = await querySelectedCommand(client, proc.pid!, frame, query);
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
