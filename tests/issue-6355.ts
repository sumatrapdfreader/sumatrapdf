// Hiding the caption preserves tab navigation and session restoration.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { ControlClient, ControlCommand, type LayoutInfo } from "./control.ts";
import { makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, waitForExit } from "./win-automation.ts";
import {
  clientToScreen,
  findChildWindow,
  getClassName,
  isWindowVisible,
  setCursorPos,
  sleep,
  windowFromPoint,
} from "./winapi.ts";

async function invoke(client: ControlClient, command: string): Promise<void> {
  const res = await client.request(ControlCommand.TestInvokeCommand, [command]);
  if (res[0] !== 0) throw new Error(`issue-6355: ${command}: ${res[1]}`);
}

async function barLayout(client: ControlClient, visible: boolean): Promise<LayoutInfo> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const layout = await client.layout();
    if (layout.items.tabs?.visible === visible) return layout;
    if (Date.now() > deadline) throw new Error(`issue-6355: caption did not become ${visible}\n${layout.raw}`);
    await sleep(40);
  }
}

async function tabPaths(client: ControlClient): Promise<string[]> {
  const res = await client.request(ControlCommand.TestMarkdownFollowLink, ["", 0]);
  if (res[0] !== 0) throw new Error(`issue-6355: could not read tabs: ${res[1]}`);
  return [...String(res[1]).matchAll(/^tab win=0 .*file=(.+\.pdf)$/gm)].map((m) => m[1]!);
}

async function currentPath(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const match = /^path=(.+) page=/.exec(String(res[1]));
  if (res[0] !== 0 || !match) throw new Error(`issue-6355: no current document: ${res[1]}`);
  return match[1]!;
}

export async function testit(): Promise<void> {
  if (IS_MAC) return;
  const dir = tmpPath("issue-6355");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const docs = [join(dir, "first.pdf"), join(dir, "second.pdf")];
  for (const doc of docs) writeFileSync(doc, makeBlankPdf(), "latin1");
  const settingsPath = join(dir, "SumatraPDF-settings.txt");
  writeFileSync(
    settingsPath,
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = true\nReuseInstance = false\n" +
      "ShowStartPage = false\nNoHomeTab = true\nUseTabs = true\nShowCaption = false\nShowMenubarWithTabs = true\n",
  );
  setCursorPos(0, 0);
  const first = await launchControlled(["-appdata", dir, ...docs], { saveSettings: true });
  try {
    await first.client.waitForRenderIdle();
    const hidden = await barLayout(first.client, false);
    if (hidden.items.menu.visible) throw new Error("issue-6355: hidden caption left the menu visible");
    if ((await tabPaths(first.client)).length !== docs.length)
      throw new Error("issue-6355: documents split into windows");
    const selected = await currentPath(first.client);
    await invoke(first.client, "CmdNextTab");
    await first.client.waitForRenderIdle();
    if ((await currentPath(first.client)) === selected) throw new Error("issue-6355: hidden tabs cannot be switched");
    await barLayout(first.client, false);
    await invoke(first.client, "CmdToggleCaption");
    const shown = await barLayout(first.client, true);
    if (shown.items.canvas.rect.dy >= hidden.items.canvas.rect.dy)
      throw new Error("issue-6355: hidden caption still reserves space");
    await invoke(first.client, "CmdToggleCaption on");
    await barLayout(first.client, true);
    await invoke(first.client, "CmdToggleCaption off");
    const beforeHover = await barLayout(first.client, false);
    const top = clientToScreen(first.frame, Math.floor(beforeHover.items.frame.rect.dx / 2), 4);
    setCursorPos(top.x, top.y);
    const overlay = await barLayout(first.client, true);
    if (JSON.stringify(overlay.items.canvas.rect) !== JSON.stringify(beforeHover.items.canvas.rect))
      throw new Error("issue-6355: caption overlay resized the document");
    setCursorPos(top.x, top.y + Math.floor(overlay.items.tabs.rect.y));
    await sleep(150);
    await barLayout(first.client, true);
    setCursorPos(0, 0);
    await barLayout(first.client, false);
    await invoke(first.client, "CmdToggleMenuBar off");
    await barLayout(first.client, false);
    await invoke(first.client, "CmdToggleMenuBar on");
    await barLayout(first.client, false);
    await invoke(first.client, "CmdToggleFullscreen on");
    await invoke(first.client, "CmdToggleFullscreen off");
    await barLayout(first.client, false);
    await invoke(first.client, "WM_CLOSE");
    if (!(await waitForExit(first.proc))) throw new Error("issue-6355: first window did not close");
  } finally {
    first.client.close();
    await killAndWait(first.proc);
  }
  const saved = readFileSync(settingsPath, "utf8");
  if (!/^UseTabs = true$/m.test(saved) || !/^ShowCaption = false$/m.test(saved))
    throw new Error("issue-6355: tab settings were not saved");
  const restored = await launchControlled(["-appdata", dir], { saveSettings: true });
  try {
    await restored.client.waitForRenderIdle();
    await barLayout(restored.client, false);
    const paths = await tabPaths(restored.client);
    if (paths.length !== docs.length || !docs.every((doc) => paths.includes(doc)))
      throw new Error(`issue-6355: restored wrong tabs: ${paths}`);
  } finally {
    restored.client.close();
    await killAndWait(restored.proc);
  }
  writeFileSync(
    settingsPath,
    "UseTabs = false\nShowCaption = true\nShowMenubar = true\nRestoreSession = false\nNoHomeTab = true\n",
  );
  const noTabs = await launchControlled(["-appdata", dir, docs[0]!]);
  try {
    await noTabs.client.waitForRenderIdle();
    const pinned = await noTabs.client.layout();
    await invoke(noTabs.client, "CmdToggleCaption off");
    const hidden = await barLayout(noTabs.client, false);
    if (hidden.items.menu.visible) throw new Error("issue-6355: native menu remained visible");
    if (hidden.items.canvas.rect.dy <= pinned.items.canvas.rect.dy)
      throw new Error("issue-6355: native caption space was not reclaimed");
    const top = clientToScreen(noTabs.frame, Math.floor(hidden.items.frame.rect.dx / 2), 4);
    setCursorPos(top.x, top.y);
    const hoverDeadline = Date.now() + 5_000;
    let overlay = await noTabs.client.layout();
    while (!overlay.items.menu.visible) {
      if (Date.now() > hoverDeadline) throw new Error(`issue-6355: caption overlay has no menu\n${overlay.raw}`);
      await sleep(40);
      overlay = await noTabs.client.layout();
    }
    setCursorPos(0, 0);
    await barLayout(noTabs.client, false);
    await invoke(noTabs.client, "CmdToggleCaption on");
    const deadline = Date.now() + 5_000;
    while ((await noTabs.client.layout()).items.canvas.rect.dy !== pinned.items.canvas.rect.dy) {
      if (Date.now() > deadline)
        throw new Error(
          `issue-6355: native caption was not restored\nbefore: ${pinned.raw}\nafter: ${(await noTabs.client.layout()).raw}`,
        );
      await sleep(40);
    }
  } finally {
    noTabs.client.close();
    await killAndWait(noTabs.proc);
  }
  const markdown = join(dir, "browser.md");
  writeFileSync(markdown, "# Browser document\n\nCaption overlay test.\n");
  writeFileSync(
    settingsPath,
    "UseTabs = true\nShowCaption = false\nShowMenubarWithTabs = true\nRestoreSession = false\nNoHomeTab = true\nMarkdownUI [\nUseFixedPageUI = false\n]\n",
  );
  const browser = await launchControlled(["-appdata", dir, markdown]);
  try {
    const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
    for (;;) {
      const ready = await browser.client.request(ControlCommand.TestMarkdownTocNavigate, [0, 0]);
      if (ready[0] === 0) break;
      if (Date.now() > deadline) throw new Error(`issue-6355: browser not ready: ${ready[1]}`);
      await sleep(40);
    }
    await invoke(browser.client, "CmdToggleToolbar off");
    const toolbarDeadline = Date.now() + 5_000;
    while ((await browser.client.layout()).items.toolbar.visible) {
      if (Date.now() > toolbarDeadline) throw new Error("issue-6355: toolbar did not hide");
      await sleep(40);
    }
    const hidden = await barLayout(browser.client, false);
    const top = clientToScreen(browser.frame, Math.floor(hidden.items.frame.rect.dx / 2), 4);
    setCursorPos(top.x, top.y);
    const overlay = await barLayout(browser.client, true);
    if (JSON.stringify(overlay.items.canvas.rect) !== JSON.stringify(hidden.items.canvas.rect))
      throw new Error("issue-6355: caption overlay resized the browser");
    const button = clientToScreen(browser.frame, 12, 12);
    setCursorPos(button.x, button.y);
    await sleep(150);
    const hit = windowFromPoint(button.x, button.y);
    if (hit !== browser.frame && getClassName(hit) !== "SumatraCaptionOverlay")
      throw new Error(`issue-6355: browser covers caption buttons: ${getClassName(hit)}`);
    setCursorPos(0, 0);
    await barLayout(browser.client, false);
  } finally {
    browser.client.close();
    await killAndWait(browser.proc);
  }
  await toggleCaptionTwice(dir, docs);
}

// Hiding the caption and showing it again must bring back the tab strip: the
// overlay hid the tabs window and the relayout only moved it. ng's tabs are
// not a window of their own.
async function toggleCaptionTwice(dir: string, docs: string[]): Promise<void> {
  if (USE_NG) return;
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UseTabs = true\nShowCaption = true\nRestoreSession = false\nNoHomeTab = true\n",
  );
  setCursorPos(0, 0);
  const app = await launchControlled(["-appdata", dir, ...docs]);
  try {
    await app.client.waitForRenderIdle();
    const tabs = findChildWindow(app.frame, "SumatraTabsCtrlClass");
    if (!tabs || !isWindowVisible(tabs)) throw new Error("issue-6355: no visible tabs at start");
    await invoke(app.client, "CmdToggleCaption");
    await barLayout(app.client, false);
    await invoke(app.client, "CmdToggleCaption");
    await barLayout(app.client, true);
    const deadline = Date.now() + 2_000 * SLOW_BUILD_FACTOR;
    while (!isWindowVisible(tabs)) {
      if (Date.now() > deadline) throw new Error("issue-6355: tabs stayed hidden after showing the caption again");
      await sleep(40);
    }
  } finally {
    app.client.close();
    await killAndWait(app.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
