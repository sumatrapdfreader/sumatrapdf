// #6246: the command palette's setting description shows only in "=" mode.
// Leaving "=" collapsed its layout box but the text kept painting.

import { ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, runStandalone } from "./util.ts";
import {
  killAndWait,
  launchControlled,
  paletteSetText,
  sendCommand,
  waitForPalette,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";
import { sleep } from "./winapi.ts";

async function waitFor(client: ControlClient, what: string, pred: (p: PaletteState) => boolean): Promise<PaletteState> {
  const deadline = Date.now() + 8000;
  for (;;) {
    const p = await paletteState(client);
    if (pred(p)) {
      return p;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6246: ${what}; palette is ${JSON.stringify(p)}`);
    }
    await sleep(100);
  }
}

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled([]);
  try {
    sendCommand(frame, cmdId("CmdCommandPalette"));
    await waitFor(client, "the palette never opened", (p) => p.open);
    // mac types into the palette through TestInput. The query edit has no HWND.
    const handles = IS_MAC ? { palette: 0, edit: 0 } : await waitForPalette(client, frame);
    if (!IS_MAC && !handles.edit) {
      throw new Error("issue-6246: no query edit");
    }

    // [query, setting help expected]: in and out of "=" through other modes
    const steps: [string, boolean][] = [
      ["=", true],
      ["", false],
      ["=", true],
      ["@", false],
      ["=", true],
      ["#", false],
    ];
    for (const [query, want] of steps) {
      await paletteSetText(client, handles, query);
      await waitFor(
        client,
        `after '${query}' the setting help ${want ? "is hidden" : "still shows"}`,
        (p) => p.open && p.queryLen === query.length && (p.settingHelp === 1) === want,
      );
    }
    console.log("issue-6246: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
