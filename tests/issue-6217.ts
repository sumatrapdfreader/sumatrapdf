// A highlight made from a text selection is one gesture, so one Undo must
// take it back and leave the document unmodified (issue #6217). Creating the
// annotation and then setting its quad points were separate journal
// operations, so it took three presses.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type State = { raw: string; annotations: number; canUndo: boolean; modified: boolean };

async function state(client: ControlClient): Promise<State> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const count = /annotations=(\d+)/.exec(raw);
    const undo = /undo canUndo=(\d) canRedo=(\d) modified=(\d)/.exec(raw);
    if (res[0] === 0 && count && undo) {
      return { raw, annotations: +count[1]!, canUndo: undo[1] === "1", modified: undo[3] === "1" };
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6217: could not read markup state\n${raw}`);
    }
    await sleep(50);
  }
}

function want(s: State, what: string, cond: boolean): void {
  if (!cond) {
    throw new Error(`issue-6217: ${what}\n${s.raw}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6217");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );
  const pdf = join(process.cwd(), "ext", "a-zlib", "zlib.3.pdf");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    await client.seedTextSelection(1);
    sendCommand(frame, cmdId("CmdCreateAnnotHighlight"));
    await sleep(300);
    await client.waitForRenderIdle();
    let s = await state(client);
    want(s, "the highlight was not created", s.annotations === 1 && s.canUndo && s.modified);

    sendCommand(frame, cmdId("CmdUndo"));
    await client.waitForRenderIdle();
    await sleep(150);
    s = await state(client);
    want(s, "one undo must remove the highlight", s.annotations === 0);
    want(s, "one undo must leave nothing else to undo", !s.canUndo);
    want(s, "one undo must leave the document unmodified", !s.modified);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6217: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
