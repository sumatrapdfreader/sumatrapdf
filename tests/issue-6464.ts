// #6464: browser-like tabs.
// - the first Home tab is only as wide as its label plus margins
// - a "+" after the tabs is always there and takes its space from them
// - "+" adds a Home tab at the end, which can be closed and isn't restored
// - that tab focuses its filter, and a file opened from it replaces it

import { copyFileSync, writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient, type VirtCtrlInfo } from "./control.ts";
import { cmdId, makeTextPdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import {
  packCoords,
  sendCopyDataW,
  sendMessage,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
  MK_LBUTTON,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const TAB_KIND = "tabCtrl";
const PLUS_KIND = "tabsPlus";
const BAR_KIND = "tabs";
const HOME_TAB_MAX_DIVISOR = 2;
const TABS_READY_MS = 10_000;
const CROWDED_DOCS = 14;
const DDE_COPYDATA_ID = 0x44646557;

const CLOSE_KIND = "virtCtrlCloseButton";

type Bar = {
  tabs: VirtCtrlInfo[];
  plus: VirtCtrlInfo | undefined;
  bar: VirtCtrlInfo | undefined;
  closes: VirtCtrlInfo[];
};

// a shown ✕ whose center is in the tab
function closeButtonIn(b: Bar, tab: VirtCtrlInfo): boolean {
  return b.closes.some((c) => {
    const x = c.rect.x + Math.floor(c.rect.dx / 2);
    return x >= tab.rect.x && x < tab.rect.x + tab.rect.dx;
  });
}

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
    closes: all.filter((c) => c.kind === CLOSE_KIND),
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

// the way a file arrives from outside: LoadDocument with no reuse requested
function openFile(frame: number, path: string): void {
  check(sendCopyDataW(frame, DDE_COPYDATA_ID, `[Open("${path}", 0, 1, 0)]`), "could not send Open");
}

async function expectSearchFocus(client: ControlClient): Promise<void> {
  const deadline = Date.now() + TABS_READY_MS;
  for (;;) {
    if ((await client.homeSelection()).searchFocus) {
      return;
    }
    check(Date.now() < deadline, "the Home filter should have focus");
    await sleep(50);
  }
}

async function waitForDocument(client: ControlClient): Promise<void> {
  const deadline = Date.now() + TABS_READY_MS;
  while (!(await onDocument(client))) {
    check(Date.now() < deadline, "the document should be the current tab");
    await sleep(50);
  }
  await client.waitForRenderIdle();
}

async function testOpenInHomeTab(paths: string[], appdata: string): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, paths[0]!]);
  try {
    await client.waitForRenderIdle();
    const homeDx = (await waitForTabs(client, 2)).tabs[0]!.rect.dx;

    sendCommandSync(frame, cmdId("CmdNewTab"));
    await waitForTabs(client, 3);
    await expectSearchFocus(client);

    // the extra Home tab turns into the document
    openFile(frame, paths[1]!);
    await waitForDocument(client);
    await waitForTabs(client, 3);

    // the pinned Home tab stays: the file gets a tab of its own
    sendCommandSync(frame, cmdId("CmdGoToHomePage"));
    await expectSearchFocus(client);
    openFile(frame, paths[2]!);
    await waitForDocument(client);
    const b = await waitForTabs(client, 4);
    check(b.tabs[0]!.rect.dx === homeDx, "the pinned Home should have stayed");
  } finally {
    await killAndWait(proc);
  }
}

// NoHomeTab: "+" still works, and opening a file leaves no Home tab behind
async function testNoHomeTab(paths: string[], appdata: string): Promise<void> {
  const withDoc = await launchControlled(["-appdata", appdata, paths[0]!]);
  try {
    await withDoc.client.waitForRenderIdle();
    const docDx = (await waitForTabs(withDoc.client, 1)).tabs[0]!.rect.dx;
    sendCommandSync(withDoc.frame, cmdId("CmdNewTab"));
    await waitForTabs(withDoc.client, 2);
    await expectSearchFocus(withDoc.client);
    openFile(withDoc.frame, paths[1]!);
    await waitForDocument(withDoc.client);
    const b = await waitForTabs(withDoc.client, 2);
    check(b.tabs[0]!.rect.dx === b.tabs[1]!.rect.dx, "neither tab should be a compact Home");
    check(b.tabs[0]!.rect.dx <= docDx, "tabs shouldn't grow");
  } finally {
    await killAndWait(withDoc.proc);
  }

  // an empty window: the Home tab it gets is replaced too
  const empty = await launchControlled(["-appdata", appdata]);
  try {
    const deadline = Date.now() + TABS_READY_MS;
    while (!(await empty.client.homeSelection()).ready) {
      check(Date.now() < deadline, "the Home page should have appeared");
      await sleep(50);
    }
    sendCommandSync(empty.frame, cmdId("CmdNewTab"));
    await expectSearchFocus(empty.client);
    openFile(empty.frame, paths[0]!);
    await waitForDocument(empty.client);
    await waitForTabs(empty.client, 1);
  } finally {
    await killAndWait(empty.proc);
  }
}

async function expectPinnedHomeKept(client: ControlClient, frame: number, homeDx: number): Promise<void> {
  sendCommandSync(frame, cmdId("CmdNewTab"));
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

    // a real click adds a selected Home tab at the end, sized to its label like
    // the pinned one but with room for its ✕, which it shows (#6469)
    clickPlus(b.plus!);
    b = await waitForTabs(client, 3);
    const extra = b.tabs[2]!;
    check(!(await onDocument(client)), "the new Home tab should be selected");
    check(
      extra.rect.dx > home!.rect.dx && extra.rect.dx < doc!.rect.dx,
      `new Home is ${extra.rect.dx}, pinned Home ${home!.rect.dx}, document ${doc!.rect.dx}`,
    );
    check(closeButtonIn(b, extra), "the new Home tab should show its ✕");
    check(!closeButtonIn(b, home!), "the pinned Home tab has no ✕");
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
    sendCommandSync(frame, cmdId("CmdNewTab"));
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
    sendCommandSync(first.frame, cmdId("CmdNewTab"));
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
  await testOpenInHomeTab(paths, writeAppdata("issue-6464-open", settings));
  await testNoHomeTab(paths, writeAppdata("issue-6464-nohome", settings + "NoHomeTab = true\n"));
  // without -for-testing, ReuseInstance would hand the launch to any running instance
  const sessionSettings = "RestoreSession = true\nReuseInstance = false\nCheckForUpdates = false\n";
  await testNotRestored(pdf, writeAppdata("issue-6464-session", sessionSettings));
}

if (import.meta.main) {
  await runStandalone(testit);
}
