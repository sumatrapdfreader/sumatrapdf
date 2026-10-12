// Typing a command palette prefix at the document opens the palette in that
// mode: ">" commands, "@" tabs, "#" history, "$" favorites, "%" table of
// contents, "&" thumbnails. They are ASCII accelerators (matched on the typed
// character, so on any keyboard layout) for CmdCommandPalette and the
// CmdNavigate* aliases. A text field keeps the character: "#" typed into the
// find box must not open the palette.
//
// Run: bun tests/palette-prefix-keys.ts [--no-build]

import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, EXE, runStandalone } from "./util.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";
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

type Palette = { open: boolean; queryLen: number; thumb: boolean };

async function paletteState(client: ControlClient): Promise<Palette> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  if (res[0] === 2) {
    return { open: false, queryLen: 0, thumb: false };
  }
  const out = String(res[1] ?? "").trim();
  const m = /queryLen=(\d+).*thumb=(\d)/.exec(out);
  if (res[0] !== 0 || !m) {
    throw new Error(`palette-prefix-keys: TestCommandPalette failed: ${out}`);
  }
  return { open: true, queryLen: +m[1]!, thumb: m[2] === "1" };
}

async function waitFor(client: ControlClient, what: string, pred: (p: Palette) => boolean): Promise<Palette> {
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
    postMessage(getFocusedHwnd(frame), WM_KEYDOWN, VK_ESCAPE, 0);
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
          (p) => p.open && p.queryLen === k.queryLen && p.thumb === k.thumb,
        );
        await closePalette(client, frame);
      }

      // the find box keeps a typed prefix
      sendCommand(frame, cmdId("CmdFindFirst"));
      await sleep(300);
      const edit = getFocusedHwnd(frame);
      // macOS has one handle for the frame, the canvas and the focus: ask the find UI
      const findUi = IS_MAC ? String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "") : "";
      const findOpen = IS_MAC ? /compact=1|floating=1/.test(findUi) : edit !== canvas;
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
