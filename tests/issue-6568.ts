// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6568
//
// A comic in Fit Width And Continuous, scrolled to page 2: Fit Page and Single
// Page went back to page 1. Switching to single page toggled the scrollbar,
// and the resize that follows restored the exact scroll position remembered
// for page 1, which isn't shown in single page mode.
//
// Run: bun tests/issue-6568.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const CBZ = join(import.meta.dir, "issue-1201.cbz");

type View = { mode: string; current: number };

async function view(client: ControlClient): Promise<View> {
  const res = await client.request(ControlCommand.TestDisplayMode, ["pages"]);
  const raw = String(res[1] ?? "");
  const m = /mode=(.+?) step=\d+ current=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6568: TestDisplayMode: ${raw.trim()}`);
  }
  return { mode: m[1]!, current: Number(m[2]) };
}

// commands are posted: poll until the view is in `mode`
async function invoke(client: ControlClient, cmd: string, mode: string): Promise<View> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  const deadline = Date.now() + 8000;
  let v = await view(client);
  while (v.mode !== mode && Date.now() < deadline) {
    await sleep(50);
    v = await view(client);
  }
  await client.waitForRenderIdle();
  return view(client);
}

export async function testit(): Promise<void> {
  const { proc, client } = await launchControlled([CBZ]);
  try {
    await client.waitForRenderIdle();
    await invoke(client, "CmdZoomFitWidthAndContinuous", "continuous");
    await client.request(ControlCommand.TestInvokeCommand, ["CmdGoToNextPage"]);
    const deadline = Date.now() + 8000;
    let v = await view(client);
    while (v.current !== 2 && Date.now() < deadline) {
      await sleep(50);
      v = await view(client);
    }
    if (v.current !== 2) {
      throw new Error(`issue-6568: next page: on page ${v.current}`);
    }
    v = await invoke(client, "CmdZoomFitPageAndSinglePage", "single page");
    if (v.current !== 2) {
      throw new Error(`issue-6568: Fit Page and Single Page went to page ${v.current}, want 2`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
