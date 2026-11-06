// Reopen Last Closed (Ctrl+Shift+T) after closing a window brings back all its
// documents, in their tab order and with the same tab selected, in a window of
// their own. Before, closing a window remembered none of its tabs.
//
// Run: bun tests/reopen-closed-window.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { enumWindows, getClassName, getWindowPid, getWindowText, postMessage, sleep, WM_CLOSE } from "./winapi.ts";

function frames(pid: number): number[] {
  const out: number[] = [];
  enumWindows((h) => {
    if (getWindowPid(h) === pid && getClassName(h) === FRAME_CLASS) {
      out.push(h);
    }
    return true;
  });
  return out;
}

async function waitFor<T>(what: string, get: () => T | null, ms = 8000): Promise<T> {
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    const v = get();
    if (v !== null) {
      return v;
    }
    await sleep(100);
  }
  throw new Error(`reopen-closed-window: ${what}`);
}

// the frame of pid that isn't in known
function newFrame(pid: number, known: number[]): number | null {
  const f = frames(pid).find((h) => !known.includes(h));
  return f ?? null;
}

function waitTitle(frame: number, name: string): Promise<string> {
  return waitFor(`window title should show ${name}: "${getWindowText(frame)}"`, () => {
    const t = getWindowText(frame);
    return t.includes(name) ? t : null;
  });
}

export async function testit(): Promise<void> {
  const dir = tmpPath("reopen-closed-window");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const names = ["doc-a.pdf", "doc-b.pdf", "doc-c.pdf"];
  for (const n of names) {
    copyFileSync(src, join(dir, n));
  }

  const { proc, client, frame } = await launchControlled([join(dir, names[0]!), join(dir, names[1]!)]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const pid = proc.pid!;
    await waitTitle(frame, "doc-b");

    // window 2: doc-b, doc-c with doc-c selected
    sendCommand(frame, cmdId("CmdDuplicateInNewWindow"));
    const frame2 = await waitFor("no second window", () => newFrame(pid, [frame]));
    await waitTitle(frame2, "doc-b");
    sendCommand(frame2, cmdId("CmdDuplicateInNewTab"));
    await sleep(500);
    sendCommand(frame2, cmdId("CmdOpenNextFileInFolder"));
    await waitTitle(frame2, "doc-c");

    // window 1 keeps only doc-a, so the reopened documents aren't open anywhere
    sendCommand(frame, cmdId("CmdCloseCurrentDocument"));
    await waitTitle(frame, "doc-a");

    postMessage(frame2, WM_CLOSE, 0, 0);
    await waitFor("window 2 did not close", () => (frames(pid).length === 1 ? true : null));

    sendCommand(frame, cmdId("CmdReopenLastClosedFile"));
    const frame3 = await waitFor("Reopen Last Closed did not open a window", () => newFrame(pid, [frame]));
    await waitTitle(frame3, "doc-c");
    sendCommand(frame3, cmdId("CmdPrevTab"));
    await waitTitle(frame3, "doc-b");
    console.log(`  reopened window: doc-b, doc-c (doc-c selected)`);

    // window 1 is unchanged
    const t1 = getWindowText(frame);
    if (!t1.includes("doc-a")) {
      throw new Error(`reopen-closed-window: window 1 should still show doc-a: "${t1}"`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
