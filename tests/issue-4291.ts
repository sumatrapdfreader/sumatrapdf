// Installer glitches from issue #4291:
//  - toggling "Install for all users" keeps a typed install directory
//  - a running SumatraPDF is asked to quit, so it saves its session, instead
//    of being terminated. That is the installer's KillProcWithId, reached
//    here through the TestQuitProcess control command of a second instance.
//
// Run: bun tests/issue-4291.ts [--no-build]
// Needs the regular (non-static) exe: SumatraPDF-static embeds no installer payload.

import { existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { basename, join } from "node:path";
import { IS_MAC } from "./host.ts";
import { ControlClient } from "./control.ts";
import { cmdId, EXE, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  findChildWindow,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sleep,
  waitForTopWindow,
  WM_CLOSE,
} from "./winapi.ts";

const INSTALLER_CLASS = USE_NG ? "GpuiSystemMonitor" : "SUMATRA_PDF_INSTALLER_FRAME";
const PROBE_CLASS = "SUMATRA_INSTALLER_PROBE";
// ng Installer.cpp kInstallerToggleAllUsers
const TOGGLE_ALL_USERS = 0x8000 + 45;
const BM_CLICK = 0x00f5;
const ALL_USERS_LABEL = "all users";
const QUIT_TIMEOUT_MS = 15000;

function samePath(a: string, b: string): boolean {
  const norm = (s: string) => s.replace(/[\\/]+$/, "").toLowerCase();
  return norm(a) === norm(b);
}

async function pollUntil<T>(timeoutMs: number, probe: () => T | undefined): Promise<T | undefined> {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    const v = probe();
    if (v !== undefined) {
      return v;
    }
    await sleep(100);
  }
  return undefined;
}

function childOf(frame: number, cls: string, label?: string): number {
  let res = 0;
  enumChildWindows(frame, (h) => {
    if (getClassName(h) !== cls) {
      return true;
    }
    if (label && !getWindowText(h).replaceAll("&", "").toLowerCase().includes(label)) {
      return true;
    }
    res = h;
    return false;
  });
  return res;
}

// orig: the options rows, then click the "all users" checkbox
async function origToggleAllUsers(frame: number): Promise<string> {
  const edit = childOf(frame, "Edit");
  const opts = childOf(frame, "Button", "options");
  if (!edit || !opts) {
    throw new Error("issue-4291: installer window lacks the Options button or the directory edit");
  }
  if (!isWindowVisible(edit)) {
    postMessage(opts, BM_CLICK, 0, 0);
    if (!(await pollUntil(5000, () => (isWindowVisible(edit) ? true : undefined)))) {
      throw new Error("issue-4291: Options did not show the directory edit");
    }
  }
  const before = getControlText(edit);
  const allUsers = childOf(frame, "Button", ALL_USERS_LABEL);
  if (!allUsers) {
    throw new Error("issue-4291: no 'Install for all users' checkbox");
  }
  postMessage(allUsers, BM_CLICK, 0, 0);
  // give the toggle time to rewrite the edit (the defect) before reading it
  await sleep(1000);
  const after = getControlText(edit);
  if (!before) {
    throw new Error("issue-4291: the directory edit is empty");
  }
  return after;
}

// ng: the probe window carries "...|dir|<path>"; a message toggles the option
async function ngToggleAllUsers(frame: number): Promise<string> {
  const probe = await pollUntil(5000, () => findChildWindow(frame, PROBE_CLASS) || undefined);
  if (!probe) {
    throw new Error("issue-4291: installer probe did not appear");
  }
  const dirOf = (): string | undefined => {
    const m = /\|dir\|(.*)$/.exec(getControlText(probe));
    return m ? m[1] : undefined;
  };
  if (!(await pollUntil(5000, dirOf))) {
    throw new Error("issue-4291: the probe does not report the install directory");
  }
  postMessage(frame, TOGGLE_ALL_USERS, 0, 0);
  await sleep(1000);
  return dirOf() ?? "";
}

async function installerKeepsTypedDir(installDir: string): Promise<void> {
  const proc = Bun.spawn([EXE, "-for-testing", "-lang", "en", "-install", "-d", installDir], {
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    const frame = await waitForTopWindow(proc.pid!, INSTALLER_CLASS, 12000);
    if (!frame) {
      throw new Error("issue-4291: installer window did not appear");
    }
    await sleep(500);
    const after = USE_NG ? await ngToggleAllUsers(frame) : await origToggleAllUsers(frame);
    if (!samePath(after, installDir)) {
      throw new Error(`issue-4291: 'Install for all users' replaced '${installDir}' with '${after}'`);
    }
    postMessage(frame, WM_CLOSE, 0, 0);
  } finally {
    await killAndWait(proc);
  }
}

function frameWindowsOf(pid: number): number[] {
  const res: number[] = [];
  enumWindows((h) => {
    if (getWindowPid(h) === pid && getClassName(h) === FRAME_CLASS && isWindowVisible(h)) {
      res.push(h);
    }
    return true;
  });
  return res;
}

// the SessionData block of a settings file: FilePath and PageNo per tab
function sessionTabs(settingsPath: string): { file: string; pageNo: number }[] {
  if (!existsSync(settingsPath)) {
    return [];
  }
  const text = readFileSync(settingsPath, "utf8");
  const start = text.indexOf("\nSessionData [");
  if (start < 0) {
    return [];
  }
  const end = text.indexOf("\n]", start);
  const block = text.slice(start, end < 0 ? undefined : end);
  const res: { file: string; pageNo: number }[] = [];
  for (const m of block.matchAll(/FilePath = (.*)\r?\n(?:.*\r?\n)*?\s*PageNo = (\d+)/g)) {
    res.push({ file: m[1]!.trim(), pageNo: Number(m[2]) });
  }
  return res;
}

// one instance with two windows is told to go away by another instance the
// way the installer does it; its saved session must list both documents at
// the pages they were on (a page change is only saved on a clean exit)
async function quitKeepsSession(appData: string): Promise<void> {
  const settingsPath = join(appData, "SumatraPDF-settings.txt");
  // both have more than one page, the next-page step must land somewhere
  const pdfs = [join(ROOT, "tests", "xmp-visagesoft.pdf"), join(ROOT, "ext", "a-zlib", "zlib.3.pdf")];
  // no -for-testing: the session must be saved. Single page view: in a
  // continuous view a next-page step can leave page 1 at the top of a big window
  const args = [EXE, "-appdata", appData, "-view", "single page", "-new-window", ...pdfs];
  const app = Bun.spawn(args, { stdout: "ignore", stderr: "ignore" });
  try {
    const frames = await pollUntil(15000, () => {
      const f = frameWindowsOf(app.pid!);
      return f.length >= 2 ? f : undefined;
    });
    if (!frames) {
      throw new Error("issue-4291: the two document windows did not appear");
    }
    await sleep(1500);
    for (const frame of frames) {
      sendCommand(frame, cmdId("CmdGoToNextPage"));
    }
    await sleep(1000);
    const ctl = await launchControlled(["-lang", "en"]);
    try {
      const gone = await ctl.client.quitProcess(app.pid!);
      if (!gone) {
        throw new Error("issue-4291: TestQuitProcess did not stop the instance");
      }
      await ctl.client.quit();
    } finally {
      await killAndWait(ctl.proc);
    }
    const exited = await Promise.race([app.exited.then(() => true), sleep(QUIT_TIMEOUT_MS).then(() => false)]);
    if (!exited) {
      throw new Error("issue-4291: the instance is still running after the quit request");
    }
  } finally {
    await killAndWait(app);
  }
  const tabs = sessionTabs(settingsPath);
  for (const pdf of pdfs) {
    const tab = tabs.find((t) => samePath(t.file, pdf));
    if (!tab) {
      throw new Error(`issue-4291: the saved session lacks ${basename(pdf)}: ${JSON.stringify(tabs)}`);
    }
    if (tab.pageNo !== 2) {
      throw new Error(`issue-4291: ${basename(pdf)} was saved at page ${tab.pageNo}, not 2`);
    }
  }
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-4291: the installer is Windows-only");
    return;
  }
  const appData = tmpPath("issue-4291-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  await quitKeepsSession(appData);

  if (/static/i.test(basename(EXE))) {
    console.log("issue-4291: skip the installer part, static exe has no installer payload");
    return;
  }
  const installDir = tmpPath("issue-4291-install");
  rmSync(installDir, { recursive: true, force: true });
  mkdirSync(installDir, { recursive: true });
  await installerKeepsTypedDir(installDir);
}

if (import.meta.main) {
  await runStandalone(testit);
}
