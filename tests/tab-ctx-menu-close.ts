// The tab context menu runs a nested message loop. A tab closed while it was
// open was then used by the picked item ("Copy File Path" read the freed tab).
import { ControlCommand } from "./control";
import { runStandalone, USE_NG } from "./util";
import {
  clientToScreen,
  clipboardSeqNo,
  isWindowVisible,
  findChildWindow,
  postMessage,
  sleep,
  VK_DOWN,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi";
import { killAndWait, launchControlled, openContextMenu, waitForContextMenu } from "./win-automation";

const kTabsClass = "SumatraTabsCtrlClass";
// "Document Properties...", "Show in folder", "Copy File Path"
const kCopyPathDowns = 3;

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("tab-ctx-menu-close: orig only (ng has no modal tab menu)");
    return;
  }
  const { proc, client, frame } = await launchControlled(["tests/issue-1189.pdf", "tests/issue-1809.pdf"]);
  try {
    await client.waitForRenderIdle();
    // tab 0 is Home
    const { count: nTabs } = await client.selectTab(1);

    const tabs = findChildWindow(frame, kTabsClass);
    if (!tabs) {
      throw new Error("tab-ctx-menu-close: no tabs window");
    }
    const tabCtrls = (await client.virtTree(kTabsClass)).filter((c) => c.kind === "tabCtrl");
    const r = tabCtrls[1]?.rect;
    if (!r) {
      throw new Error("tab-ctx-menu-close: no rect for tab 1");
    }
    const pt = clientToScreen(tabs, r.x + Math.floor(r.dx / 2), r.y + Math.floor(r.dy / 2));
    openContextMenu(tabs, pt.x, pt.y);
    const popup = await waitForContextMenu(3000);
    if (!popup) {
      throw new Error("tab-ctx-menu-close: tab context menu did not open");
    }

    // close the tab under the menu, then pick "Copy File Path" for it
    await client.request(ControlCommand.TestInvokeCommand, ["CmdClose"]);
    const deadline = Date.now() + 3000;
    while ((await client.selectTab(-1)).count !== nTabs - 1) {
      if (Date.now() > deadline) {
        throw new Error("tab-ctx-menu-close: tab did not close while the menu was open");
      }
      await sleep(50);
    }
    if (!isWindowVisible(popup)) {
      throw new Error("tab-ctx-menu-close: closing the tab dismissed the menu");
    }
    const clipSeq = clipboardSeqNo();
    for (let i = 0; i < kCopyPathDowns; i++) {
      postMessage(popup, WM_KEYDOWN, VK_DOWN, 0);
    }
    postMessage(popup, WM_KEYDOWN, VK_RETURN, 0);
    await sleep(300);

    let count = -1;
    try {
      count = (await client.selectTab(-1)).count;
    } catch (e) {
      throw new Error(`tab-ctx-menu-close: SumatraPDF crashed using the closed tab: ${e}`);
    }
    if (count !== nTabs - 1) {
      throw new Error(`tab-ctx-menu-close: expected ${nTabs - 1} tabs, got ${count}`);
    }
    // the closed tab's path must not be copied
    if (clipboardSeqNo() !== clipSeq) {
      throw new Error("tab-ctx-menu-close: Copy File Path ran for the closed tab");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
