// #6464: browser-like tabs.
// - the first Home tab is only as wide as its label plus margins
// - a "+" after the tabs is always there and takes its space from them
// - "+" adds a Home tab at the end, which can be closed and isn't restored

import { copyFileSync, writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient, type VirtCtrlInfo } from "./control.ts";
import { cmdId, makeTextPdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { packCoords, sendMessage, sleep, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, MK_LBUTTON } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const TAB_KIND = "tabCtrl";
const PLUS_KIND = "tabsPlus";
const BAR_KIND = "tabs";
const HOME_TAB_MAX_DIVISOR = 2;
const TABS_READY_MS = 10_000;
const CROWDED_DOCS = 14;

type Bar = { tabs: VirtCtrlInfo[]; plus: VirtCtrlInfo | undefined; bar: VirtCtrlInfo | undefined };

function check(cond: boolean, what: string): void {
  if (!cond) {
    throw new Error(`issue-6464: ${what}`);
  }
}

async function readBar(client: ControlClient): Promise<Bar> {
  const all = await client.virtTree();
  return {
    tabs: all.filter((c) => c.kind === TAB_KIND).sort((a, b) => a.rect.x - b.rect.x),
    plus: all.find((c) => c.kind === PLUS_KIND),
    bar: all.find((c) => c.kind === BAR_KIND),
  };
}

async function waitForTabs(client: ControlClient, want: number): Promise<Bar> {
  const deadline = Date.now() + TABS_READY_MS;
  for (;;) {
    const b = await readBar(client);
    if (b.tabs.length === want) {
      return b;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6464: want ${want} tabs, got ${b.tabs.length}`);
    }
    await sleep(50);
  }
}

// true when the current tab shows a document, false on Home
async function onDocument(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return res[0] === 0;
}

function clickPlus(plus: VirtCtrlInfo): void {
  const x = plus.rect.x + Math.floor(plus.rect.dx / 2);
  const y = plus.rect.y + Math.floor(plus.rect.dy / 2);
  const pt = packCoords(x, y);
  sendMessage(plus.hwnd, WM_MOUSEMOVE, 0, pt);
  sendMessage(plus.hwnd, WM_LBUTTONDOWN, MK_LBUTTON, pt);
  sendMessage(plus.hwnd, WM_LBUTTONUP, 0, pt);
}

async function expectPinnedHomeKept(client: ControlClient, frame: number, homeDx: number): Promise<void> {
  sendCommandSync(frame, cmdId("CmdNewHomeTab"));
  const b = await waitForTabs(client, 2);
  check(b.tabs[0]!.rect.dx === homeDx, "the pinned Home should have stayed");
}

async function testLayoutAndPlus(pdf: string, appdata: string): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    let b = await waitForTabs(client, 2);
    const [home, doc] = b.tabs;
    check(home!.rect.dx * HOME_TAB_MAX_DIVISOR < doc!.rect.dx, `Home is ${home!.rect.dx}, document ${doc!.rect.dx}`);
    check(doc!.rect.x === home!.rect.x + home!.rect.dx, "document tab should follow Home");
    check(!!b.plus, "no + button");
    check(b.plus!.rect.x === doc!.rect.x + doc!.rect.dx, "+ should follow the last tab");

    // a real click adds a selected, regular-width Home tab at the end
    clickPlus(b.plus!);
    b = await waitForTabs(client, 3);
    const extra = b.tabs[2]!;
    check(!(await onDocument(client)), "the new Home tab should be selected");
    check(extra.rect.dx === doc!.rect.dx, `new Home is ${extra.rect.dx}, document ${doc!.rect.dx}`);
    check(b.plus!.rect.x === extra.rect.x + extra.rect.dx, "+ should follow the new tab");

    // the pinned Home neither moves nor lets anything take its place
    sendCommandSync(frame, cmdId("CmdMoveTabLeft"));
    sendCommandSync(frame, cmdId("CmdMoveTabLeft"));
    b = await waitForTabs(client, 3);
    check(b.tabs[0]!.rect.dx === home!.rect.dx, "pinned Home should stay first");

    // closing the document leaves two Home tabs, and the window stays
    sendCommandSync(frame, cmdId("CmdNextTab"));
    check(await onDocument(client), "should be on the document");
    sendCommandSync(frame, cmdId("CmdClose"));
    b = await waitForTabs(client, 2);

    // closing the extra Home tab keeps the pinned one: it isn't "the last document".
    // A lone Home hides the bar, so its presence shows when the next Home appears
    check(b.tabs[1]!.rect.dx !== home!.rect.dx, "should be on the extra Home");
    sendCommandSync(frame, cmdId("CmdClose"));
    await expectPinnedHomeKept(client, frame, home!.rect.dx);

    // "close all" takes extra Home tabs along and keeps the pinned one
    sendCommandSync(frame, cmdId("CmdNewHomeTab"));
    await waitForTabs(client, 3);
    sendCommandSync(frame, cmdId("CmdCloseAllTabs"));
    await expectPinnedHomeKept(client, frame, home!.rect.dx);
  } finally {
    await killAndWait(proc);
  }
}

async function testCrowded(paths: string[], appdata: string): Promise<void> {
  const { proc, client } = await launchControlled(["-appdata", appdata, ...paths]);
  try {
    await client.waitForRenderIdle();
    const b = await waitForTabs(client, paths.length + 1);
    const plusEnd = b.plus!.rect.x + b.plus!.rect.dx;
    check(
      plusEnd <= b.bar!.rect.x + b.bar!.rect.dx,
      `+ ends at ${plusEnd}, past the bar ${JSON.stringify(b.bar!.rect)}`,
    );
  } finally {
    await killAndWait(proc);
  }
}

async function testNotRestored(pdf: string, appdata: string): Promise<void> {
  const first = await launchControlled(["-appdata", appdata, pdf], { saveSettings: true });
  try {
    await first.client.waitForRenderIdle();
    sendCommandSync(first.frame, cmdId("CmdNewHomeTab"));
    await waitForTabs(first.client, 3);
    await first.client.quit();
  } finally {
    await killAndWait(first.proc);
  }

  const second = await launchControlled(["-appdata", appdata], { saveSettings: true });
  try {
    await second.client.waitForRenderIdle();
    await waitForTabs(second.client, 2);
    check(await onDocument(second.client), "the restored document should be selected");
  } finally {
    await killAndWait(second.proc);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6464.pdf");
  writeFileSync(pdf, makeTextPdf("home tab width"));
  const paths = [pdf];
  for (let i = 1; i < CROWDED_DOCS; i++) {
    const p = tmpPath(`issue-6464-${i}.pdf`);
    copyFileSync(pdf, p);
    paths.push(p);
  }
  const settings = "RestoreSession = false\nCheckForUpdates = false\n";
  await testLayoutAndPlus(pdf, writeAppdata("issue-6464-appdata", settings));
  await testCrowded(paths, writeAppdata("issue-6464-crowded", settings));
  await testNotRestored(pdf, writeAppdata("issue-6464-session", "RestoreSession = true\nCheckForUpdates = false\n"));
}

if (import.meta.main) {
  await runStandalone(testit);
}
