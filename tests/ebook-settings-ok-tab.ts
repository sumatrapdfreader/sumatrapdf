// eBook Settings is modeless. Its OK must re-layout the ebook it was opened
// for, not whatever tab is current by then: reloading a PDF dropped its
// unsaved annotations without a prompt (round 5 review C13).
//
// Run: bun tests/ebook-settings-ok-tab.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { makeEpub } from "./epub-metadata-properties.ts";
import { cmdId, makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, pressEnter, sendCommandSync } from "./win-automation.ts";
import { enumWindows, getWindowPid, getWindowText, packCoords, sendMessage, WM_COMMAND } from "./winapi.ts";

const kDlgTitle = "eBook Settings";
const kToolWin = "ebooksettings";
const kRoleButton = 18;

// annotations in the current document
async function annotCount(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "");
  const m = /OK n=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`ebook-settings-ok-tab: no annotation state: ${raw}`);
  }
  return Number(m[1]);
}

function findDlg(pid: number): number {
  let found = 0;
  enumWindows((h) => {
    if (getWindowPid(h) === pid && getWindowText(h) === kDlgTitle) {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

// presses the dialog's OK
async function pressOk(client: ControlClient, pid: number): Promise<void> {
  if (!USE_NG) {
    const dlg = await pollUntil(
      async () => findDlg(pid),
      (h) => h !== 0,
      { error: "ebook-settings-ok-tab: eBook Settings did not open" },
    );
    await pressEnter(dlg);
    await pollUntil(
      async () => findDlg(pid),
      (h) => h === 0,
      { error: "ebook-settings-ok-tab: Enter did not close eBook Settings" },
    );
    return;
  }
  const toolWin = async (...args: (string | number)[]) =>
    String((await client.request(ControlCommand.TestToolWindow, args))[1] ?? "");
  const re = /node rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) role=(\d+) label='OK' value=/;
  const m = await pollUntil(
    async () => re.exec(await toolWin("layout", kToolWin)),
    (r) => r !== null && Number(r[5]) === kRoleButton,
    { error: "ebook-settings-ok-tab: eBook Settings did not open" },
  );
  const x = Math.round(Number(m![1]) + Number(m![3]) / 2);
  const y = Math.round(Number(m![2]) + Number(m![4]) / 2);
  await toolWin("input", kToolWin, "click", x, y, 0, 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ebook-settings-ok-tab");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const epub = join(dir, "book.epub");
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  writeFileSync(epub, makeEpub());

  const { proc, client, frame } = await launchControlled([pdf, epub]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // open the dialog on the ebook, then switch to the PDF and annotate it
    sendCommandSync(frame, cmdId("CmdChangeEbookSettings"));
    sendCommandSync(frame, cmdId("CmdPrevTab"));
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotText"), packCoords(150, 300));
    await client.waitForRenderIdle();
    if ((await annotCount(client)) !== 1) {
      throw new Error("ebook-settings-ok-tab: could not add an annotation to the PDF");
    }

    await pressOk(client, proc.pid!);
    await client.waitForRenderIdle();
    const n = await annotCount(client);
    if (n !== 1) {
      throw new Error(`ebook-settings-ok-tab: OK reloaded the PDF, its unsaved annotation is gone (n=${n})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
