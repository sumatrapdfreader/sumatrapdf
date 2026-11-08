// The compact find bar and the floating find window are alternatives, but
// each show function only knew its own: with SearchUIFloating changed while
// the bar was up (settings file, a search started from outside), both showed.
//
// Run: bun tests/find-ui-one-at-a-time.ts [--no-build] [-ng]

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { pollUntil, ROOT, runStandalone, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const PDF = join(ROOT, "tests", "issue-2423.pdf");

async function findUi(client: ControlClient): Promise<{ compact: number; floating: number; raw: string }> {
  const raw = String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "");
  const m = /compact=(\d+) floating=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`find-ui-one-at-a-time: no find UI state: ${raw}`);
  }
  return { compact: +m[1]!, floating: +m[2]!, raw };
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "find-ui-one-at-a-time",
    ["UiLanguage = en", "CheckForUpdates = false", "SearchUIFloating = false"].join("\n"),
  );
  const { proc, client } = await launchControlled(["-appdata", appdata, PDF]);
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestInvokeCommand, ["CmdFindFirst"]);
    await pollUntil(
      async () => findUi(client),
      (s) => s.compact === 1 && s.floating === 0,
      { error: (s) => `find-ui-one-at-a-time: Ctrl+F didn't show the compact bar: ${s.raw}` },
    );

    // the hook switches to the floating window and searches there
    await client.request(ControlCommand.TestFindResultsOrder, ["the", 1]);
    await pollUntil(
      async () => findUi(client),
      (s) => s.floating === 1,
      { error: (s) => `find-ui-one-at-a-time: the floating window didn't show: ${s.raw}` },
    );
    const s = await findUi(client);
    if (s.compact !== 0) {
      throw new Error(`find-ui-one-at-a-time: both find UIs are up: ${s.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("find-ui-one-at-a-time: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
