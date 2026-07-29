// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/2254
//
// FB2 <title-info><annotation>…</annotation> must show in Document Properties
// (Ctrl+D) as Subject, alongside Title and Author.
//
// Fixture: tests/issue-2254.fb2
// Run:  bun tests/issue-2254.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, runStandalone } from "./util.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  postMessage,
  sendMessage,
  sleep,
  WM_CLOSE,
  WM_GETTEXT,
  WM_GETTEXTLENGTH,
} from "./winapi.ts";
import { launchSumatra, sendCommand, waitForFrame } from "./win-automation.ts";
import { ptr } from "bun:ffi";

const FB2 = join(import.meta.dir, "issue-2254.fb2");

async function waitForTopWindowTitle(pid: number, titleSubstr: string, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let found = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) !== pid) {
        return true;
      }
      const title = getWindowText(hwnd);
      if (title.includes(titleSubstr)) {
        found = hwnd;
        return false;
      }
      return true;
    });
    if (found) {
      return found;
    }
    await sleep(100);
  }
  return 0;
}

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

function getEditText(hwnd: number): string {
  const tlen = Number(sendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0));
  if (tlen <= 0) {
    return "";
  }
  const buf = new Uint16Array(tlen + 2);
  sendMessage(hwnd, WM_GETTEXT, tlen + 1, BigInt(ptr(buf)));
  let s = "";
  for (let i = 0; i < tlen; i++) {
    if (buf[i] === 0) {
      break;
    }
    s += String.fromCharCode(buf[i]);
  }
  return s;
}

export async function testit(): Promise<void> {
  if (!existsSync(FB2)) {
    throw new Error(`fixture missing: ${FB2}`);
  }
  if (!existsSync(EXE)) {
    throw new Error(`exe missing: ${EXE}`);
  }

  const proc = launchSumatra([FB2]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error("issue-2254: no frame window");
    }
    await sleep(2000); // let FB2 load / reflow

    sendCommand(frame, cmdId("CmdProperties"));
    const props = await waitForTopWindowTitle(proc.pid!, "Document Properties");
    if (!props) {
      throw new Error("issue-2254: Document Properties window did not open");
    }

    const edit = findFirstEdit(props);
    if (!edit) {
      throw new Error("issue-2254: no Edit control in Document Properties");
    }
    await sleep(200);
    const text = getEditText(edit);
    console.log(`issue-2254 properties text (${text.length} chars):\n${text.slice(0, 800)}`);

    if (!text.includes("Annotation Prop Test")) {
      throw new Error(`issue-2254: missing Title in properties:\n${text}`);
    }
    if (!/Subject:\s*Unique annotation marker for issue 2254/.test(text)) {
      throw new Error(`issue-2254: missing Subject from FB2 annotation:\n${text}`);
    }
    if (!/Author:\s*Test Author/.test(text)) {
      throw new Error(`issue-2254: missing Author in properties:\n${text}`);
    }

    postMessage(props, WM_CLOSE, 0, 0);
    await sleep(200);
  } finally {
    try {
      proc.kill();
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
