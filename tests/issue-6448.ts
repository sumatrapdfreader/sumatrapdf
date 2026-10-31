// #6448: opening a .html file showed every .html file in its folder as pages of
// one document. HTML now opens as a single file (Markdown keeps folder mode).
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { cmdId, runStandalone, tmpPath, USE_NG } from "./util";
import { findChildWindow, sendMessage, sleep, treeExpandRecursively, TVE_EXPAND, TVM_GETCOUNT } from "./winapi";
import {
  killAndWait,
  launchControlled,
  launchSumatra,
  sendCommand,
  waitForFrame,
  waitForTitle,
} from "./win-automation";

async function tocItemCount(frame: number, client: ControlClient | null): Promise<number> {
  if (client) {
    const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
    const m = /tocItems=(-?\d+)/.exec(raw);
    return m ? +m[1]! : -1;
  }
  const tree = findChildWindow(frame, "SysTreeView32");
  if (!tree) {
    return -1;
  }
  treeExpandRecursively(tree, TVE_EXPAND);
  return Number(sendMessage(tree, TVM_GETCOUNT, 0, 0));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6448");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (const name of ["abc", "xyz"]) {
    const body = `<h1>${name} title</h1><p>text</p><h2>${name} section</h2><p>text</p>`;
    writeFileSync(join(dir, `${name}.html`), `<html><body>${body}</body></html>\n`);
  }

  const path = join(dir, "abc.html");
  let proc: Bun.Subprocess;
  let client: ControlClient | null = null;
  let frame = 0;
  if (USE_NG) {
    ({ proc, client, frame } = await launchControlled([path]));
  } else {
    proc = launchSumatra([path]);
  }
  try {
    frame = frame || (await waitForFrame(proc.pid!));
    await waitForTitle(frame, (t) => t.startsWith("abc.html"), 30000);
    sendCommand(frame, cmdId("CmdToggleBookmarks"));

    // wait for the background build to settle: the same count twice in a row
    const deadline = Date.now() + 15000;
    let last = -1;
    let n = -1;
    while (Date.now() < deadline) {
      n = await tocItemCount(frame, client);
      if (n > 0 && n === last) {
        break;
      }
      last = n;
      await sleep(500);
    }
    // the file plus its 2 headings; xyz.html must not be there
    const want = 3;
    if (n !== want) {
      throw new Error(`issue-6448: TOC has ${n} items, want ${want} (abc.html only, no sibling xyz.html)`);
    }
    console.log("issue-6448: OK");
  } finally {
    client?.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
