// #6464: the Home tab is only as wide as its label plus margins. Document tabs
// keep TabWidth and start right after it.

import { writeFileSync } from "node:fs";
import type { ControlClient, VirtCtrlInfo } from "./control.ts";
import { makeTextPdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const TAB_KIND = "tabCtrl";
const HOME_TAB_MAX_DIVISOR = 2;
const TABS_READY_MS = 10_000;

async function waitForTabs(client: ControlClient, want: number): Promise<VirtCtrlInfo[]> {
  const deadline = Date.now() + TABS_READY_MS;
  for (;;) {
    const all = await client.virtTree();
    const tabs = all.filter((c) => c.kind === TAB_KIND).sort((a, b) => a.rect.x - b.rect.x);
    if (tabs.length === want) {
      return tabs;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6464: want ${want} tabs, got ${tabs.length}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6464.pdf");
  writeFileSync(pdf, makeTextPdf("home tab width"));
  const appdata = writeAppdata("issue-6464-appdata", "RestoreSession = false\nCheckForUpdates = false\n");

  const { proc, client } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const [home, doc] = await waitForTabs(client, 2);
    if (home!.rect.dx * HOME_TAB_MAX_DIVISOR >= doc!.rect.dx) {
      throw new Error(`issue-6464: Home tab is ${home!.rect.dx} wide, document tab ${doc!.rect.dx}`);
    }
    if (doc!.rect.x !== home!.rect.x + home!.rect.dx) {
      throw new Error(`issue-6464: document tab doesn't follow Home: ${JSON.stringify([home!.rect, doc!.rect])}`);
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
