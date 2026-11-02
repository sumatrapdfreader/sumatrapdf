// #6469, three of its items:
// 2: the Rotate buttons' tooltips showed the menu's "&" ("Rotate &Left")
// 4: Close Other Tabs (or Left / Right) from a command left the tab bar blank:
//    the strip rebuilt its tabs and was never repainted
// 5: Go To Home Page and New Tab were missing from the command palette
//
// Run: bun tests/issue-6469.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findChildByClass, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { getClientRect, readWindowDCRow } from "./winapi.ts";

const TABS_CLASS = "SumatraTabsCtrlClass";
// a blank strip is one color; tabs add their labels and backgrounds
const MIN_TAB_COLORS = 3;

async function checkTooltips(client: ControlClient): Promise<void> {
  const raw = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
  const withAmp = raw.split("\n").filter((l) => /^idx=.* text=.*&/.test(l));
  if (withAmp.length > 0) {
    throw new Error(`issue-6469: tooltips with '&':\n${withAmp.join("\n")}`);
  }
}

async function checkPalette(client: ControlClient): Promise<void> {
  for (const cmd of ["CmdGoToHomePage", "CmdNewTab"]) {
    const raw = String((await client.request(ControlCommand.TestCommandVisibility, [cmd, "palette"]))[1] ?? "");
    if (!/vis=show/.test(raw)) {
      throw new Error(`issue-6469: ${cmd} isn't in the command palette: ${raw.trim()}`);
    }
  }
}

// the strip as the screen has it: PrintWindow would paint it afresh and hide the bug
function tabColors(tabs: number): number {
  const rc = getClientRect(tabs);
  const row = readWindowDCRow(tabs, 0, Math.floor((rc.bottom - rc.top) / 2), rc.right - rc.left);
  return new Set(row).size;
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP issue-6469: orig's toolbar and tab strip");
    return;
  }
  const files = ["a", "b", "c"].map((n) => {
    const p = tmpPath(`issue-6469-${n}.pdf`);
    writeFileSync(p, makePdf(2, 600, 0, 0), "latin1");
    return p;
  });
  const { proc, client, frame } = await launchControlled(files);
  try {
    await client.waitForRenderIdle();
    await checkTooltips(client);
    await checkPalette(client);

    const tabs = findChildByClass(frame, TABS_CLASS);
    if (!tabs) {
      throw new Error("issue-6469: no tab strip");
    }
    await pollUntil(
      () => tabColors(tabs),
      (n) => n >= MIN_TAB_COLORS,
      { error: (n) => `issue-6469: the tab strip isn't drawn before closing (${n} colors)` },
    );
    sendCommand(frame, cmdId("CmdCloseOtherTabs"));
    await pollUntil(
      async () => (await client.request(ControlCommand.TestCurrentTab, []))[1],
      () => true,
    );
    await pollUntil(
      () => tabColors(tabs),
      (n) => n >= MIN_TAB_COLORS,
      { error: (n) => `issue-6469: Close Other Tabs left the tab strip blank (${n} colors)` },
    );
    console.log("issue-6469: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
