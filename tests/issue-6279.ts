// Regression test for discussion #6279: renaming (F2) a file opened from a
// UNC path left the tab showing the old name instead of the renamed document.
//
// Uses the administrative share of the repo's drive (\\localhost\d$\...), so
// it skips where that share is not reachable.
//
// Run:  bun tests/issue-6279.ts [--no-build]

import { existsSync, rmSync, writeFileSync } from "node:fs";
import { basename } from "node:path";
import { cmdId, makeMinimalPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, pressKey, sendCommand, waitForTitle } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  getControlText,
  sendMessage,
  VK_RETURN,
  WM_CHAR,
} from "./winapi.ts";

const kOldName = "issue-6279-old";
const kNewName = "issue-6279-new";
const kDialogClass = "#32770";
const EM_SETSEL = 0x00b1;

// D:\dir\file.pdf => \\localhost\d$\dir\file.pdf
function toUncPath(path: string): string {
  return `\\\\localhost\\${path[0].toLowerCase()}$${path.slice(2)}`;
}

function findRenameDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || getClassName(hwnd) !== kDialogClass) {
      return true;
    }
    found = hwnd;
    return false;
  });
  return found;
}

// the file name edit is the one pre-filled with the current name
function findFileNameEdit(dlg: number): number {
  let found = 0;
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) !== "Edit" || getControlText(hwnd) !== kOldName) {
      return true;
    }
    found = hwnd;
    return false;
  });
  return found;
}

// the file dialog ignores WM_SETTEXT on its file name edit, so type over it
function typeOverText(edit: number, text: string): void {
  sendMessage(edit, EM_SETSEL, 0, -1);
  for (const ch of text) {
    sendMessage(edit, WM_CHAR, ch.charCodeAt(0), 0);
  }
}

export async function testit(): Promise<void> {
  const oldPath = tmpPath(`${kOldName}.pdf`);
  const newPath = tmpPath(`${kNewName}.pdf`);
  const oldUnc = toUncPath(oldPath);
  rmSync(newPath, { force: true });
  writeFileSync(oldPath, makeMinimalPdf("issue 6279"));

  if (!existsSync(oldUnc)) {
    console.log(`\nSKIP issue-6279: '${oldUnc}' is not reachable (needs the administrative share)`);
    return;
  }

  const { proc, client, frame } = await launchControlled([oldUnc]);
  try {
    await client.waitForRenderIdle();

    sendCommand(frame, cmdId("CmdRenameFile"));
    const edit = await pollUntil(
      () => {
        const dlg = findRenameDialog(proc.pid!);
        return dlg ? findFileNameEdit(dlg) : 0;
      },
      (hwnd) => hwnd !== 0,
      { timeoutMs: 10_000, error: "the Rename To dialog did not show up" },
    );
    typeOverText(edit, kNewName);
    await pressKey(edit, VK_RETURN);

    await pollUntil(
      () => existsSync(newPath) && !existsSync(oldPath),
      (renamed) => renamed,
      { timeoutMs: 10_000, error: "the file was not renamed on disk" },
    );
    await waitForTitle(frame, (title) => title.includes(basename(newPath)));
    // fails when the tab is stuck loading / failed to load
    await client.waitForRenderIdle();
  } finally {
    await client.quit();
    rmSync(oldPath, { force: true });
    rmSync(newPath, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
