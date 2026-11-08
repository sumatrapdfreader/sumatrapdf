// Reloading a document in presentation or in F11 with Fullscreen.DisplayMode
// keeps their layout, and leaving them afterwards restores the document's own.
// The reload builds a new DisplayModel from the saved (non-fullscreen) state.
//
// Run: bun tests/reload-keeps-fullscreen-layout.ts [--no-build] [-ng]

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF = join(ROOT, "tests", "issue-2423.pdf");

async function mode(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "");
  const m = /mode=(.+?) presentation=/.exec(raw);
  if (!m) {
    throw new Error(`reload-keeps-fullscreen-layout: no display mode: ${raw}`);
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
      throw new Error(`reload-keeps-fullscreen-layout: ${what}: mode=${got}, want ${want}`);
    }
    await sleep(50);
  }
}

// reload, then give the new document time to settle before reading its mode
async function reload(client: ControlClient): Promise<void> {
  await cmd(client, "CmdReloadDocument");
  await client.waitForRenderIdle();
  await sleep(200);
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "reload-keeps-fullscreen-layout",
    ["UiLanguage = en", "CheckForUpdates = false", "Fullscreen [", "  DisplayMode = facing", "]"].join("\n"),
  );
  const { proc, client } = await launchControlled(["-appdata", appdata, "-view", "continuous", PDF]);
  try {
    await client.waitForRenderIdle();
    await waitMode(client, "continuous", "start");

    await cmd(client, "CmdTogglePresentationMode");
    await waitMode(client, "facing", "presentation");
    await reload(client);
    await waitMode(client, "facing", "presentation after reload");
    await cmd(client, "CmdTogglePresentationMode");
    await waitMode(client, "continuous", "after presentation");

    await cmd(client, "CmdToggleFullscreen");
    await waitMode(client, "facing", "fullscreen");
    await reload(client);
    await waitMode(client, "facing", "fullscreen after reload");
    await cmd(client, "CmdToggleFullscreen");
    await waitMode(client, "continuous", "after fullscreen");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("reload-keeps-fullscreen-layout: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
