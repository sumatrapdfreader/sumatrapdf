// #6467: Highlight fills a Ctrl+drag rectangle, as it did in 3.5.2. Select All
// and the line-shaped markups have no use for a rectangle and stay no-ops.
//
// A real Ctrl key state can't be injected here; TestDrawRectSelection runs the
// same OnSelectionStart / OnSelectionStop calls a Ctrl+drag does.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makeTextPdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const WORD = "grabme";
const SELECTION_READY_MS = 15_000;

async function markupState(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  return String(res[1] ?? "");
}

function annotCount(raw: string): number {
  const m = /annotations=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6467: no annotation count\n${raw}`);
  }
  return +m[1]!;
}

async function selectRect(client: ControlClient): Promise<void> {
  const deadline = Date.now() + SELECTION_READY_MS;
  for (;;) {
    const res = await client.request(ControlCommand.TestDrawRectSelection, [WORD]);
    const raw = String(res[1] ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      if (res[0] !== 0) {
        throw new Error(`issue-6467: rectangular selection failed: ${raw}`);
      }
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6467: document never became ready: ${raw}`);
    }
    await sleep(100);
  }
}

// commands are sent synchronously, so the count is final when this reads it
async function expectCountAfter(client: ControlClient, frame: number, cmd: string, want: number, what: string) {
  sendCommandSync(frame, cmdId(cmd));
  const raw = await markupState(client);
  const got = annotCount(raw);
  if (got !== want) {
    throw new Error(`issue-6467: ${what}: want ${want} annotations, got ${got}\n${raw}`);
  }
  return raw;
}

async function expectMenuVis(client: ControlClient, cmd: string, want: string): Promise<void> {
  const res = await client.request(ControlCommand.TestCommandVisibility, [cmd, "menu"]);
  const got = /vis=(\w+)/.exec(String(res[1] ?? ""))?.[1];
  if (got !== want) {
    throw new Error(`issue-6467: ${cmd} in menu: want ${want}, got ${got}`);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6467.pdf");
  writeFileSync(pdf, makeTextPdf(`${WORD} in the middle`));
  const appdata = writeAppdata("issue-6467-appdata", "RestoreSession = false\nCheckForUpdates = false\n");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdSelectAll"));
    await expectMenuVis(client, "CmdCreateAnnotHighlight", "disable");
    await expectCountAfter(client, frame, "CmdCreateAnnotHighlight", 0, "highlight on Select All");

    await selectRect(client);
    await expectMenuVis(client, "CmdCreateAnnotHighlight", "show");
    await expectMenuVis(client, "CmdCreateAnnotUnderline", "disable");
    await expectCountAfter(client, frame, "CmdCreateAnnotUnderline", 0, "underline on a rectangle");
    await expectCountAfter(client, frame, "CmdCreateAnnotStrikeOut", 0, "strike out on a rectangle");
    await expectCountAfter(client, frame, "CmdCreateAnnotSquiggly", 0, "squiggly on a rectangle");

    const raw = await expectCountAfter(client, frame, "CmdCreateAnnotHighlight", 1, "highlight on a rectangle");
    if (!/type=Highlight page=1 quads=1\b/.test(raw)) {
      throw new Error(`issue-6467: want one Highlight quad on page 1\n${raw}`);
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
