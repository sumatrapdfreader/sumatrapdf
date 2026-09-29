// #6246: the command palette's setting description shows only in "=" mode.
// Leaving "=" collapsed its layout box but the text kept painting.

import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { getFocusedHwnd, sendText, sleep } from "./winapi.ts";

type Palette = { open: boolean; queryLen: number; settingHelp: boolean };

async function paletteState(client: ControlClient): Promise<Palette> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const out = String(res[1] ?? "").trim();
  if (res[0] === 2) {
    return { open: false, queryLen: 0, settingHelp: false };
  }
  const m = /queryLen=(\d+) .* settingHelp=(\d)/.exec(out);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6246: TestCommandPalette failed: ${out}`);
  }
  return { open: true, queryLen: +m[1]!, settingHelp: m[2] === "1" };
}

async function waitFor(client: ControlClient, what: string, pred: (p: Palette) => boolean): Promise<Palette> {
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
    const edit = getFocusedHwnd(frame);
    if (!edit) {
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
      sendText(edit, query);
      await waitFor(
        client,
        `after '${query}' the setting help ${want ? "is hidden" : "still shows"}`,
        (p) => p.open && p.queryLen === query.length && p.settingHelp === want,
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
