// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5993
//
// The View Certificate and Update EU Trusted List actions in Document
// Properties are PDF-specific. They must not appear for a comic book.
//
// Run: bun tests/issue-5993.ts [--no-build]

import { join } from "node:path";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";
import { clickAt, killAndWait, launchControlled, pressKey, sendCommandSync } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getControlText,
  getFocusedHwnd,
  getWindowPid,
  getWindowText,
  sleep,
  VK_TAB,
} from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const CBZ = join(ROOT, "tests", "issue-1201.cbz");
const kWaitMs = 8000 * SLOW_BUILD_FACTOR;
const kTabMs = 150 * SLOW_BUILD_FACTOR;

function findFirstEdit(parent: number): number {
  let found = 0;
  enumChildWindows(parent, (hwnd) => {
    if (getClassName(hwnd) === "Edit") {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForProperties(pid: number, timeoutMs = kWaitMs): Promise<{ props: number; edit: number }> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let props = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) === pid && getWindowText(hwnd).includes("Document Properties")) {
        props = hwnd;
        return false;
      }
      return true;
    });
    const edit = props ? findFirstEdit(props) : 0;
    if (props && edit) {
      return { props, edit };
    }
    await sleep(50);
  }
  throw new Error("issue-5993: Document Properties window did not open");
}

async function waitForHwndFocus(hwndInThread: number, expected: number, what: string): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  let focused = 0;
  while (Date.now() < deadline) {
    focused = getFocusedHwnd(hwndInThread);
    if (focused === expected) {
      return;
    }
    await sleep(40);
  }
  const cls = focused ? getClassName(focused) : "none";
  throw new Error(`issue-5993: ${what} (focus=${focused} class=${cls}, expected=${expected})`);
}

// SizeToContent after the font list arrives can take longer than a fixed Tab
// settle, especially on ASan, so wait until that work has finished.
async function waitForFontList(edit: number): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  while (Date.now() < deadline) {
    const text = getControlText(edit);
    if (text && !text.includes("Getting font information...")) {
      return;
    }
    await sleep(40);
  }
  throw new Error("issue-5993: properties font list did not finish");
}

async function checkPropertiesActions(path: string, isPdf: boolean): Promise<void> {
  const { proc, client, frame } = await launchControlled([path]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdProperties"));
    const { props, edit } = await waitForProperties(proc.pid!);
    await waitForFontList(edit);

    // Focus the native Edit first so the number of Tabs needed to wrap is
    // deterministic. Each drawn action is one tab stop hosted by props itself.
    const rc = getClientRect(edit);
    await clickAt(edit, Math.min(10, rc.right - 1), Math.min(10, rc.bottom - 1), 100 * SLOW_BUILD_FACTOR);
    await waitForHwndFocus(props, edit, "could not focus the properties text");

    await pressKey(props, VK_TAB, 0); // Copy To Clipboard
    await waitForHwndFocus(props, props, "Copy To Clipboard was not the next tab stop");
    await pressKey(props, VK_TAB, kTabMs);
    if (isPdf) {
      // View Certificate and Update EU Trusted List follow Copy; the fourth
      // Tab wraps to the Edit.
      if (getFocusedHwnd(props) !== props) {
        throw new Error("issue-5993: PDF certification actions are missing");
      }
      await pressKey(props, VK_TAB, kTabMs);
      await pressKey(props, VK_TAB, 0);
      await waitForHwndFocus(props, edit, "unexpected PDF properties action count");
    } else {
      await waitForHwndFocus(props, edit, "PDF certification actions are visible for a comic book");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await checkPropertiesActions(PDF, true);
  await checkPropertiesActions(CBZ, false);
  console.log("PASS: PDF certification actions only appear in PDF properties (issue #5993)");
}

if (import.meta.main) {
  await runStandalone(testit);
}
