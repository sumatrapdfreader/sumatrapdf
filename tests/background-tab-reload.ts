// A document changed on disk while its tab is in the background reloads when
// the tab is selected. ng armed its reload delay only for the current tab, so
// the background tab kept showing the old document.
//
// Run: bun tests/background-tab-reload.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

async function currentTab(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return String(res[1] ?? "");
}

async function cmd(client: ControlClient, name: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [name]);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("background-tab-reload");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfA = join(dir, "a.pdf");
  const pdfB = join(dir, "b.pdf");
  writeFileSync(pdfA, makePdf(1, 400, 0, 0), "latin1");
  writeFileSync(pdfB, makePdf(1, 400, 0, 0), "latin1");

  const { proc, client } = await launchControlled([pdfA, pdfB]);
  try {
    await client.waitForRenderIdle();
    if (!(await currentTab(client)).includes("b.pdf")) {
      throw new Error(`background-tab-reload: b.pdf is not current: ${await currentTab(client)}`);
    }

    // a.pdf grows to 3 pages while b.pdf is shown; let the watcher see it
    writeFileSync(pdfA, makePdf(3, 400, 0, 0), "latin1");
    await sleep(1500);

    await cmd(client, "CmdPrevTab");
    const deadline = Date.now() + 5000;
    let last = "";
    for (;;) {
      await cmd(client, "CmdGoToLastPage");
      last = await currentTab(client);
      if (last.includes("a.pdf") && /page=3\b/.test(last)) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`background-tab-reload: a.pdf wasn't reloaded on focus (${last.trim()})`);
      }
      await sleep(100);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("background-tab-reload: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
