// Switching tabs in presentation mode shows the new tab's document in the
// presentation layout too, and leaving presentation restores the layout of a
// tab that was shown in it. ng kept each document in the layout it had.
//
// Run: bun tests/tab-switch-presentation.ts [--no-build] [-ng]

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF1 = join(ROOT, "tests", "issue-2423.pdf");
const PDF2 = join(ROOT, "tests", "issue-1189.pdf");

async function mode(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "");
  const m = /mode=(.+?) presentation=/.exec(raw);
  if (!m) {
    throw new Error(`tab-switch-presentation: no display mode: ${raw}`);
  }
  return m[1]!;
}

async function cmd(client: ControlClient, name: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [name]);
}

async function waitMode(client: ControlClient, want: string, what: string): Promise<void> {
  const deadline = Date.now() + 5000;
  let got = "";
  for (;;) {
    got = await mode(client);
    if (got === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`tab-switch-presentation: ${what}: mode=${got}, want ${want}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const { proc, client } = await launchControlled(["-view", "continuous", PDF1, PDF2]);
  try {
    await client.waitForRenderIdle();
    await waitMode(client, "continuous", "start");

    await cmd(client, "CmdTogglePresentationMode");
    await waitMode(client, "single page", "presentation");

    // tabs: (Home,) PDF1, PDF2 (current)
    await cmd(client, "CmdPrevTab");
    await waitMode(client, "single page", "other tab in presentation");

    await cmd(client, "CmdTogglePresentationMode");
    await waitMode(client, "continuous", "after presentation");
    await cmd(client, "CmdNextTab");
    await waitMode(client, "continuous", "first tab after presentation");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("tab-switch-presentation: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
