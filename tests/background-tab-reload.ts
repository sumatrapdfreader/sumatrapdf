// File changes reload the active tab and reload a background tab when selected.
//
// Run: bun tests/background-tab-reload.ts [--no-build] [-ng]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
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

async function waitForLastPage(client: ControlClient, path: string, page: number): Promise<void> {
  const deadline = Date.now() + 5000;
  for (;;) {
    await cmd(client, "CmdGoToLastPage");
    const tab = (await currentTab(client)).trimEnd();
    if (tab.includes(path) && tab.endsWith(` page=${page}`)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`background-tab-reload: expected ${path} to reload to ${page} pages (${tab})`);
    }
    await sleep(100);
  }
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

    const replacement = join(dir, "replacement.pdf");
    writeFileSync(replacement, makePdf(2, 400, 0, 0), "latin1");
    copyFileSync(replacement, pdfB);
    await waitForLastPage(client, "b.pdf", 2);

    // a.pdf grows to 3 pages while b.pdf is shown; let the watcher see it
    writeFileSync(pdfA, makePdf(3, 400, 0, 0), "latin1");
    await sleep(1500);

    await cmd(client, "CmdPrevTab");
    await waitForLastPage(client, "a.pdf", 3);
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
