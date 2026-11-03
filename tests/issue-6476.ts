// #6476: TabsEqualWidth = false sizes a tab to its file name, up to TabWidth.
// Tabs that don't fit shrink like equal-width ones, except a tab whose name
// needs less keeps its smaller width.
//
// Run: bun tests/issue-6476.ts [--no-build] [-ng]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { makeTextPdf, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { getWindowRect, moveWindow } from "./winapi.ts";

const SHORT = "a";
const MEDIUM = "medium-name";
const LONG = "a-file-name-long-enough-to-need-more-than-the-tab-width-allows";
const TAB_WIDTH = 300;
const WIDE_DX = 1600;
const SHRINK_STEP_DX = 20;
const MIN_WINDOW_DX = 200;
const SETTLE_MS = 100;

type Tab = { dx: number; text: string };

async function tabWidths(client: ControlClient): Promise<Tab[]> {
  const res = await client.request(ControlCommand.TestTabWidths, []);
  const out = String(res[1] ?? "");
  return [...out.matchAll(/^dx=(\d+) text=(.*)$/gm)].map((m) => ({ dx: +m[1]!, text: m[2]! }));
}

function byName(tabs: Tab[], name: string): number {
  const t = tabs.find((t) => t.text === `issue-6476-${name}.pdf`);
  if (!t) {
    throw new Error(`issue-6476: no tab for ${name}: ${JSON.stringify(tabs)}`);
  }
  return t.dx;
}

async function waitForTabs(client: ControlClient, ok: (tabs: Tab[]) => boolean, what: string): Promise<Tab[]> {
  return pollUntil(
    () => tabWidths(client),
    (tabs) => tabs.length >= 4 && ok(tabs),
    { error: (tabs) => `issue-6476: ${what}: ${JSON.stringify(tabs)}` },
  );
}

async function run(equalWidth: boolean): Promise<void> {
  const files = [SHORT, MEDIUM, LONG].map((n) => {
    const p = tmpPath(`issue-6476-${n}.pdf`);
    writeFileSync(p, makeTextPdf("x"));
    return p;
  });
  const appdata = writeAppdata(
    `issue-6476-appdata-${equalWidth}`,
    `TabsEqualWidth = ${equalWidth}\nTabWidth = ${TAB_WIDTH}\nCheckForUpdates = false\n`,
  );
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, ...files]);
  try {
    const rc = getWindowRect(frame);
    moveWindow(frame, rc.left, rc.top, WIDE_DX, rc.bottom - rc.top);
    if (equalWidth) {
      await waitForTabs(
        client,
        (t) => byName(t, SHORT) === byName(t, LONG) && byName(t, MEDIUM) === byName(t, LONG),
        "equal-width tabs differ",
      );
      return;
    }

    // room for all: each as wide as its name, the long one capped
    const wide = await waitForTabs(
      client,
      (t) => byName(t, SHORT) < byName(t, MEDIUM) && byName(t, MEDIUM) < byName(t, LONG),
      "tabs aren't sized to their names",
    );
    const shortDx = byName(wide, SHORT);
    const capDx = byName(wide, LONG);

    // shrink until the medium tab has to give up width too: the short one keeps
    // its width, the other two share the rest equally
    const mediumDx = byName(wide, MEDIUM);
    const allDx = wide.reduce((sum, t) => sum + t.dx, 0);
    for (let dx = allDx + SHRINK_STEP_DX * 5; ; dx -= SHRINK_STEP_DX) {
      if (dx < MIN_WINDOW_DX) {
        throw new Error(`issue-6476: the medium tab never shrank: ${JSON.stringify(await tabWidths(client))}`);
      }
      moveWindow(frame, rc.left, rc.top, dx, rc.bottom - rc.top);
      const t = await stableTabs(client);
      if (byName(t, MEDIUM) >= mediumDx) {
        continue;
      }
      if (byName(t, SHORT) !== shortDx || Math.abs(byName(t, MEDIUM) - byName(t, LONG)) > 1) {
        throw new Error(`issue-6476: shrunk tabs aren't fair-shared at ${dx}: ${JSON.stringify(t)}`);
      }
      return;
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// a resize is laid out asynchronously: wait for two equal readings in a row
async function stableTabs(client: ControlClient): Promise<Tab[]> {
  let prev = "";
  for (;;) {
    await Bun.sleep(SETTLE_MS);
    const tabs = await tabWidths(client);
    const cur = JSON.stringify(tabs);
    if (tabs.length >= 4 && cur === prev) {
      return tabs;
    }
    prev = cur;
  }
}

export async function testit(): Promise<void> {
  await run(true);
  await run(false);
}

if (import.meta.main) {
  await runStandalone(testit);
}
