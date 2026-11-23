// ng: a modal tool dialog shown from a second window while the first window
// still has it up replaces it. It used to open a second tool window and leave
// the first one behind, empty and keeping its window disabled.
//
// Run: bun tests/ng-dialog-second-window.ts -ng [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

type Win = { id: number; owner: number; name: string };

async function windows(client: ControlClient): Promise<Win[]> {
  const res = await client.request(ControlCommand.TestWindows, []);
  const out: Win[] = [];
  for (const line of String(res[1] ?? "").split("\n")) {
    const m = /^hwnd=(\d+) .*?(?:owner=(\d+) name=(\S+) )?title=/.exec(line);
    if (m) {
      out.push({ id: +m[1]!, owner: m[2] ? +m[2] : 0, name: m[3] ?? "" });
    }
  }
  return out;
}

async function invokeIn(client: ControlClient, frameId: number, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestWindows, ["target", frameId]);
  try {
    await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  } finally {
    await client.request(ControlCommand.TestWindows, ["target", 0]);
  }
}

async function closeTool(client: ControlClient, name: string): Promise<void> {
  await client.request(ControlCommand.TestToolWindow, ["close", name]);
  await pollUntil(
    () => windows(client),
    (ws) => !ws.some((w) => w.name === name),
    { error: () => `ng-dialog-second-window: ${name} did not close` },
  );
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP ng-dialog-second-window: ng's tool windows");
    return;
  }
  const pdf = tmpPath("ng-dialog-second-window.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestInvokeCommand, ["CmdDuplicateInNewWindow"]);
    const frames = await pollUntil(
      async () => (await windows(client)).filter((w) => w.name === ""),
      (fs) => fs.length === 2,
      { error: (fs) => `ng-dialog-second-window: want 2 frames, got ${fs.length}` },
    );
    const [a, b] = [frames[0]!.id, frames[1]!.id];

    const dialogs: [string, string][] = [
      ["CmdChangeScrollbar", "changescrollbar"],
      ["CmdOpenFromURL", "openfromurl"],
      ["CmdEditFavorites", "addfavorite"],
    ];
    for (const [cmd, name] of dialogs) {
      await invokeIn(client, a, cmd);
      await pollUntil(
        () => windows(client),
        (ws) => ws.some((w) => w.name === name && w.owner === a),
        { error: () => `ng-dialog-second-window: ${cmd} did not open in the first window` },
      );
      await invokeIn(client, b, cmd);
      const ws = await pollUntil(
        () => windows(client),
        (ws) => ws.some((w) => w.name === name && w.owner === b),
        { error: () => `ng-dialog-second-window: ${cmd} did not open in the second window` },
      );
      const left = ws.filter((w) => w.name === name);
      console.log(`ng-dialog-second-window: ${cmd}: ${JSON.stringify(left)}`);
      if (left.length !== 1) {
        throw new Error(`ng-dialog-second-window: ${cmd} left ${left.length} dialogs: ${JSON.stringify(left)}`);
      }
      await closeTool(client, name);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
