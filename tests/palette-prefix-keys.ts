// Typing a command palette prefix at the document opens the palette in that
// mode: ">" commands, "@" tabs, "#" history, "$" favorites, "%" table of
// contents, "&" thumbnails. They are ASCII accelerators (matched on the typed
// character, so on any keyboard layout) for CmdCommandPalette and the
// CmdNavigate* aliases. A text field keeps the character: "#" typed into the
// find box must not open the palette.
//
// Run: bun tests/palette-prefix-keys.ts [--no-build]

import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { cmdId, EXE, runStandalone, USE_NG } from "./util.ts";
import {
  findCanvas,
  findPalette,
  sendCommand,
  waitForFrame,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";
import { WM_CHAR, WM_KEYDOWN, VK_ESCAPE, getFocusedHwnd, postMessage, sleep } from "./winapi.ts";
const PDF = "ext/a-zlib/zlib.3.pdf";

// prefix key => the query length the palette shows (the default mode shows
// no prefix) and whether it is the thumbnail grid
const KEYS: { key: string; queryLen: number; thumb: boolean }[] = [
  { key: ">", queryLen: 0, thumb: false },
  { key: "@", queryLen: 1, thumb: false },
  { key: "#", queryLen: 1, thumb: false },
  { key: "$", queryLen: 1, thumb: false },
  { key: "%", queryLen: 1, thumb: false },
  { key: "&", queryLen: 1, thumb: true },
];

async function waitFor(client: ControlClient, what: string, pred: (p: PaletteState) => boolean): Promise<PaletteState> {
  const deadline = Date.now() + 8000;
  let last = await paletteState(client);
  while (!pred(last)) {
    if (Date.now() > deadline) {
      throw new Error(`palette-prefix-keys: ${what}; palette is ${JSON.stringify(last)}`);
    }
    await sleep(40);
    last = await paletteState(client);
  }
  return last;
}

async function closePalette(client: ControlClient, frame: number): Promise<void> {
  // the thumbnail grid takes one Esc for the grid and one for the palette
  for (let i = 0; i < 3 && (await paletteState(client)).open; i++) {
    postMessage(findPalette(frame).edit || getFocusedHwnd(frame), WM_KEYDOWN, VK_ESCAPE, 0);
    await sleep(150);
  }
  await waitFor(client, "Esc did not close the palette", (p) => !p.open);
}

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      const canvas = findCanvas(frame);
      for (const k of KEYS) {
        postMessage(canvas, WM_CHAR, k.key.charCodeAt(0), 0);
        await waitFor(
          client,
          `'${k.key}' did not open the palette with queryLen ${k.queryLen}, thumb ${k.thumb}`,
          (p) => p.open && p.queryLen === k.queryLen && (p.thumb === 1) === k.thumb,
        );
        await closePalette(client, frame);
      }

      // the find box keeps a typed prefix
      sendCommand(frame, cmdId("CmdFindFirst"));
      await sleep(300);
      const edit = getFocusedHwnd(frame);
      // ng has one handle for the frame, the canvas and the focus: ask the find UI
      const findUi = USE_NG ? String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "") : "";
      const findOpen = USE_NG ? /compact=1|floating=1/.test(findUi) : edit !== canvas;
      if (!edit || !findOpen) {
        throw new Error("palette-prefix-keys: the find box did not take the focus");
      }
      postMessage(edit, WM_CHAR, "#".charCodeAt(0), 0);
      await sleep(400);
      if ((await paletteState(client)).open) {
        throw new Error("palette-prefix-keys: '#' typed into the find box opened the palette");
      }
      console.log("palette-prefix-keys: OK");
    },
    ["-window-pos", "1000x700@40x40", PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
