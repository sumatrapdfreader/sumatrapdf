// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6118
//
// With EscToExit on, Esc while adding an annotation cancelled the placement,
// and the next Esc closed the document. Esc is the cancel key the placement
// notification advertises, so while the Edit PDF toolbar is up it must never
// also quit.
//
// Run: bun tests/issue-6118.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { getFocusedHwnd, isWindowVisible, postMessage, sleep, WM_KEYDOWN } from "./winapi";
import { clickAt, findChildByClass, killAndWait, launchControlled, sendCommand } from "./win-automation";

const VK_ESCAPE = 0x1b;
const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

async function placementActive(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /textPlacement active=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6118: could not read the placement state\n${raw}`);
  }
  return m[1] === "1";
}

// Only WM_KEYDOWN: the app's own message loop calls TranslateMessage, which
// makes the WM_CHAR. Posting both would be two Esc presses, not one.
async function pressEsc(hwnd: number): Promise<void> {
  postMessage(hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
  await sleep(700 * SLOW_BUILD_FACTOR);
}

export async function testit(): Promise<void> {
  const appData = tmpPath("issue-6118");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(appData, "doc.pdf");
  copyFileSync(SRC_PDF, pdf);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "EscToExit = true", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appData, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(400 * SLOW_BUILD_FACTOR);

    // start placement from the toolbar button, the way a user does
    const dump = String((await client.request(ControlCommand.TestToolbarButtons, []))[1] ?? "");
    const id = cmdId("CmdCreateAnnotText");
    const re = new RegExp(`annotation-idx=\\d+ cmd=${id} hidden=0 enabled=1 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`);
    const m = re.exec(dump);
    if (!m) {
      throw new Error(`issue-6118: Text toolbar button not found\n${dump}`);
    }
    const x = +m[1]!;
    const y = +m[2]!;
    const toolbar = findChildByClass(frame, "SUMATRA_VIRT_TOOLBAR");
    await clickAt(toolbar, x + Math.floor((+m[3]! - x) / 2), y + Math.floor((+m[4]! - y) / 2), 400);
    if (!(await placementActive(client))) {
      throw new Error("issue-6118: the toolbar button did not start placement mode");
    }

    const focus = getFocusedHwnd(frame);
    await pressEsc(focus);
    if (await placementActive(client)) {
      throw new Error("issue-6118: Esc did not cancel the placement");
    }
    if (!isWindowVisible(frame)) {
      throw new Error("issue-6118: the Esc that cancelled the placement also closed the document");
    }

    // the press after the cancel is the one that used to quit
    await pressEsc(focus);
    if (!isWindowVisible(frame) || proc.exitCode !== null) {
      throw new Error("issue-6118: Esc closed the document while the Edit PDF toolbar was up");
    }

    // EscToExit still works once Edit PDF is off
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(400 * SLOW_BUILD_FACTOR);
    await pressEsc(getFocusedHwnd(frame));
    if (isWindowVisible(frame)) {
      throw new Error("issue-6118: EscToExit stopped working outside Edit PDF mode");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
