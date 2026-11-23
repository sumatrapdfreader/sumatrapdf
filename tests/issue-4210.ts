// eBook Settings has a Page row: the page an ebook is laid out into, as a
// preset (Fit window, A5, A4, US Letter) or a custom width x height (#4210).
// Picking A4 for this file stores EBookUI.LayoutDx / LayoutDy in its FileStates
// block, lays the ebook out on A4-shaped pages and shows A4 when reopened.
//
// Run: bun tests/issue-4210.ts [--no-build] [-ng]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { makeEpub } from "./epub-metadata-properties.ts";
import { cmdId, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, pressEnter, sendCommandSync } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  sendMessage,
  sleep,
  WM_COMMAND,
} from "./winapi.ts";

const kDlgTitle = "eBook Settings";
const kToolWin = "ebooksettings";
const kRoleButton = 18;
const CB_GETCOUNT = 0x146;
const CB_GETCURSEL = 0x147;
const CB_SETCURSEL = 0x14e;
const CBN_SELCHANGE = 1;
// the Page drop-down's entries, in order
const kPageSizes = ["Fit window", "A5", "A4", "US Letter", "Custom"];
const kA4 = 2;
const kA4Ratio = 842 / 595;

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

// orig: the Page drop-down is the combo box with one entry per page size
function findPageCombo(dlg: number): number {
  let found = 0;
  enumChildWindows(dlg, (h) => {
    if (getClassName(h) === "ComboBox" && Number(sendMessage(h, CB_GETCOUNT, 0, 0)) === kPageSizes.length) {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

async function toolWin(client: ControlClient, ...args: (string | number)[]): Promise<string> {
  return String((await client.request(ControlCommand.TestToolWindow, args))[1] ?? "");
}

// ng: the center of the dialog node matching re
async function nodeCenter(client: ControlClient, re: RegExp, what: string): Promise<{ x: number; y: number }> {
  const m = await pollUntil(
    async () => re.exec(await toolWin(client, "layout", kToolWin)),
    (r) => r !== null,
    { error: `issue-4210: ${what} not found` },
  );
  return { x: Math.round(Number(m![1]) + Number(m![3]) / 2), y: Math.round(Number(m![2]) + Number(m![4]) / 2) };
}

async function clickNode(client: ControlClient, re: RegExp, what: string): Promise<void> {
  const c = await nodeCenter(client, re, what);
  await toolWin(client, "input", kToolWin, "click", c.x, c.y, 0, 0);
}

const pageSelectRe = (value: string) =>
  new RegExp(`node rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+) role=\\d+ label='' value='${value}'`);
const labelRe = (label: string, role = "\\d+") =>
  new RegExp(`node rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+) role=${role} label='${label}' value=`);

// the page size the open dialog shows
async function shownPageSize(client: ControlClient, pid: number): Promise<string> {
  if (!USE_NG) {
    const dlg = await pollUntil(
      async () => findDlg(pid),
      (h) => h !== 0,
      { error: "issue-4210: no dialog" },
    );
    const combo = findPageCombo(dlg);
    if (!combo) {
      throw new Error("issue-4210: eBook Settings has no Page drop-down");
    }
    return kPageSizes[Number(sendMessage(combo, CB_GETCURSEL, 0, 0))] ?? "?";
  }
  for (const s of kPageSizes) {
    if (pageSelectRe(s).test(await toolWin(client, "layout", kToolWin))) {
      return s;
    }
  }
  return "?";
}

async function pickA4AndOk(client: ControlClient, pid: number): Promise<void> {
  if (!USE_NG) {
    const dlg = await pollUntil(
      async () => findDlg(pid),
      (h) => h !== 0,
      { error: "issue-4210: no dialog" },
    );
    const combo = findPageCombo(dlg);
    sendMessage(combo, CB_SETCURSEL, kA4, 0);
    // what the combo tells its parent when the user picks an entry
    sendMessage(dlg, WM_COMMAND, BigInt(CBN_SELCHANGE << 16), BigInt(combo));
    await pressEnter(dlg);
    await pollUntil(
      async () => findDlg(pid),
      (h) => h === 0,
      { error: "issue-4210: OK did not close the dialog" },
    );
    return;
  }
  await clickNode(client, pageSelectRe("Fit window"), "the Page drop-down");
  await clickNode(client, labelRe("A4"), "the A4 entry");
  await clickNode(client, labelRe("OK", String(kRoleButton)), "OK");
}

async function page1Ratio(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestLayout, ["get"]))[1] ?? "");
  const m = /page n=1 shown=\d pos=-?\d+,-?\d+,(\d+),(\d+)/.exec(raw);
  return m ? Number(m[2]) / Number(m[1]) : 0;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-4210");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const epub = join(dir, "book.epub");
  writeFileSync(epub, makeEpub());
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  writeFileSync(settingsPath, "UiLanguage = en\nRestoreSession = false\nCheckForUpdates = false\n");

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, epub], { saveSettings: true });
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdChangeEbookSettings"));
    const before = await pollUntil(
      () => shownPageSize(client, pid),
      (s) => s !== "?",
      { error: "issue-4210: eBook Settings did not open" },
    );
    if (before !== "Fit window") {
      throw new Error(`issue-4210: the default page size shows as ${before}, want Fit window`);
    }
    await pickA4AndOk(client, pid);

    // stored for this file only, and the ebook laid out again on A4 pages
    await pollUntil(
      async () => (existsSync(settingsPath) ? readFileSync(settingsPath, "utf8") : ""),
      (s) => /FileStates[\s\S]*EBookUI \[[^\]]*LayoutDx = 595[^\]]*LayoutDy = 842/.test(s),
      { error: () => `issue-4210: no per-file LayoutDx / LayoutDy:\n${readFileSync(settingsPath, "utf8")}` },
    );
    const ratio = await pollUntil(
      async () => {
        await client.waitForRenderIdle();
        return page1Ratio(client);
      },
      (r) => Math.abs(r - kA4Ratio) < 0.02,
      {
        error: (r: number) => `issue-4210: page 1 is ${r.toFixed(3)} tall per width, want A4's ${kA4Ratio.toFixed(3)}`,
      },
    );
    console.log(`  A4 page: ${ratio.toFixed(3)}`);

    // and the dialog shows it next time
    sendCommandSync(frame, cmdId("CmdChangeEbookSettings"));
    await sleep(300);
    const shown = await pollUntil(
      () => shownPageSize(client, pid),
      (s) => s !== "?",
      { error: "issue-4210: eBook Settings did not reopen" },
    );
    if (shown !== "A4") {
      throw new Error(`issue-4210: reopened, the page size shows as ${shown}, want A4`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-4210: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
