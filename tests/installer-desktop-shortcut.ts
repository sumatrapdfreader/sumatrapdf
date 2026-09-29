// Installer option "Install desktop shortcut" (discussion #6255):
//  - it is the third option checkbox, checked by default
//  - -no-desktop-shortcut unchecks it
//  - the option checkboxes don't overlap and fit above the button row
//
// Only drives the installer window; it never installs. The default can come
// from a previous installation's DesktopShortcut registry value, so the
// "checked by default" part is skipped when this machine's install turned it off.
//
// Run: bun tests/installer-desktop-shortcut.ts [--no-build]
// Needs the regular (non-static) exe: SumatraPDF-static embeds no installer payload.

import { mkdirSync, rmSync } from "node:fs";
import { basename } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { killAndWait } from "./win-automation.ts";
import {
  clientToScreen,
  enumChildWindows,
  getClassName,
  getClientRect,
  getWindowRect,
  getWindowLong,
  getWindowText,
  GWL_STYLE,
  isWindowVisible,
  postMessage,
  sendMessage,
  sleep,
  waitForTopWindow,
  WM_CLOSE,
  type Rect,
} from "./winapi.ts";

const INSTALLER_CLASS = "SUMATRA_PDF_INSTALLER_FRAME";
const BM_GETCHECK = 0x00f0;
const BM_CLICK = 0x00f5;
const BST_CHECKED = 1;
const BS_TYPEMASK = 0x0f;
const BS_AUTOCHECKBOX = 0x03;
const DESKTOP_LABEL = "desktop shortcut";

type Ctrl = { hwnd: number; text: string; r: Rect };

function buttons(frame: number): Ctrl[] {
  const res: Ctrl[] = [];
  enumChildWindows(frame, (h) => {
    if (getClassName(h) === "Button") {
      res.push({ hwnd: h, text: getWindowText(h).replaceAll("&", ""), r: getWindowRect(h) });
    }
    return true;
  });
  return res;
}

function isCheckbox(c: Ctrl): boolean {
  return (getWindowLong(c.hwnd, GWL_STYLE) & BS_TYPEMASK) === BS_AUTOCHECKBOX;
}

// the visible option checkboxes, top to bottom, after the Options button showed them
async function showOptions(frame: number): Promise<Ctrl[]> {
  const opts = buttons(frame).find((b) => b.text.endsWith("Options"));
  if (!opts) {
    throw new Error("installer-desktop-shortcut: no Options button");
  }
  const visibleChecks = () => buttons(frame).filter((b) => isCheckbox(b) && isWindowVisible(b.hwnd));
  if (visibleChecks().length === 0) {
    postMessage(opts.hwnd, BM_CLICK, 0, 0);
  }
  for (let i = 0; i < 50 && visibleChecks().length === 0; i++) {
    await sleep(100);
  }
  const checks = visibleChecks().sort((a, b) => a.r.top - b.r.top);
  if (checks.length === 0) {
    throw new Error("installer-desktop-shortcut: Options did not show the option checkboxes");
  }
  return checks;
}

function checkLayout(frame: number, checks: Ctrl[]): void {
  const client = getClientRect(frame);
  const origin = clientToScreen(frame, 0, 0);
  const opts = buttons(frame).find((b) => b.text.endsWith("Options"))!;
  for (let i = 0; i < checks.length; i++) {
    const c = checks[i]!;
    if (c.r.bottom - c.r.top <= 0 || c.r.right - c.r.left <= 0) {
      throw new Error(`installer-desktop-shortcut: '${c.text}' has an empty rect`);
    }
    if (c.r.left < origin.x || c.r.right > origin.x + client.right) {
      throw new Error(`installer-desktop-shortcut: '${c.text}' sticks out of the window`);
    }
    if (i > 0 && c.r.top < checks[i - 1]!.r.bottom) {
      throw new Error(`installer-desktop-shortcut: '${c.text}' overlaps '${checks[i - 1]!.text}'`);
    }
  }
  const last = checks[checks.length - 1]!;
  if (last.r.bottom > opts.r.top) {
    throw new Error(`installer-desktop-shortcut: '${last.text}' overlaps the button row`);
  }
}

// DesktopShortcut = 0 from a previous installation on this machine
function prevInstallTurnedItOff(): boolean {
  for (const root of ["HKCU", "HKLM"]) {
    const p = Bun.spawnSync([
      "reg",
      "query",
      `${root}\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\SumatraPDF`,
      "/v",
      "DesktopShortcut",
    ]);
    if (/DesktopShortcut\s+REG_DWORD\s+0x0\b/.test(p.stdout.toString())) {
      return true;
    }
  }
  return false;
}

async function runInstaller(installDir: string, extra: string[]): Promise<Ctrl> {
  const proc = Bun.spawn([EXE, "-for-testing", "-lang", "en", "-install", "-d", installDir, ...extra], {
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    const frame = await waitForTopWindow(proc.pid!, INSTALLER_CLASS, 12000);
    if (!frame) {
      throw new Error("installer-desktop-shortcut: installer window did not appear");
    }
    await sleep(500);
    const checks = await showOptions(frame);
    const idx = checks.findIndex((c) => c.text.toLowerCase().includes(DESKTOP_LABEL));
    if (idx < 0) {
      throw new Error(
        `installer-desktop-shortcut: no desktop shortcut option in ${JSON.stringify(checks.map((c) => c.text))}`,
      );
    }
    if (checks.length >= 3 && idx !== 2) {
      throw new Error(`installer-desktop-shortcut: desktop shortcut is option ${idx + 1}, not the third`);
    }
    checkLayout(frame, checks);
    const res = checks[idx]!;
    res.text = Number(sendMessage(res.hwnd, BM_GETCHECK, 0, 0)) === BST_CHECKED ? "checked" : "unchecked";
    postMessage(frame, WM_CLOSE, 0, 0);
    return res;
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (/static/i.test(basename(EXE))) {
    console.log("skip installer-desktop-shortcut: static exe has no installer payload");
    return;
  }
  const installDir = tmpPath("installer-desktop-shortcut");
  rmSync(installDir, { recursive: true, force: true });
  mkdirSync(installDir, { recursive: true });

  const off = await runInstaller(installDir, ["-no-desktop-shortcut"]);
  if (off.text !== "unchecked") {
    throw new Error("installer-desktop-shortcut: -no-desktop-shortcut left the option checked");
  }
  if (prevInstallTurnedItOff()) {
    console.log("installer-desktop-shortcut: skip default check, this machine's install has DesktopShortcut = 0");
    return;
  }
  const def = await runInstaller(installDir, []);
  if (def.text !== "checked") {
    throw new Error("installer-desktop-shortcut: the option is not checked by default");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
