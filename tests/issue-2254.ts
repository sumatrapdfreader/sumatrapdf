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
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation.ts";
import { ptr } from "bun:ffi";

const FB2 = join(import.meta.dir, "issue-2254.fb2");

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

// PropertiesWnd::Create sets the title before creating the Edit child. Polling only
// for the title can observe the shell without an Edit (race → flaky "no Edit control").
// Wait until both the titled top-level window and its Edit exist.
async function waitForPropertiesEdit(pid: number, timeoutMs = 8000): Promise<{ props: number; edit: number }> {
  const deadline = Date.now() + timeoutMs;
  let lastProps = 0;
  while (Date.now() < deadline) {
    let props = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) !== pid) {
        return true;
      }
      if (getWindowText(hwnd).includes("Document Properties")) {
        props = hwnd;
        return false;
      }
      return true;
    });
    if (props) {
      lastProps = props;
      const edit = findFirstEdit(props);
      if (edit) {
        return { props, edit };
      }
    }
    await sleep(100);
  }
  return { props: lastProps, edit: 0 };
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

  const { proc, client, frame } = await launchControlled([FB2]);
  try {
    await client.waitForRenderIdle();

    sendCommandSync(frame, cmdId("CmdProperties"));
    const { props, edit } = await waitForPropertiesEdit(proc.pid!);
    if (!props) {
      throw new Error("issue-2254: Document Properties window did not open");
    }
    if (!edit) {
      throw new Error("issue-2254: no Edit control in Document Properties");
    }
    // Text is set during Create; retry briefly if WM_GETTEXT races SetPropsText.
    let text = "";
    for (let i = 0; i < 20; i++) {
      text = getEditText(edit);
      if (text.includes("Title:") || text.includes("Author:")) {
        break;
      }
      await sleep(100);
    }
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
  } finally {
    client.close();
    try {
      await killAndWait(proc);
    } catch {
      // already exited
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
