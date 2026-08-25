// #6008: About window copies program/machine info to the clipboard, and
// labels the date as "built on" without a "Built: " prefix.
//
// Run: bun tests/issue-6008.ts [--no-build]

import { cmdId, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";
import { getClientRect, getWindowPid, getWindowText, sleep, waitForTopWindow, waitForWindowIdle } from "./winapi.ts";
import { clickAt, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const ABOUT_CLASS = "SUMATRA_PDF_ABOUT";

function clipboardText(): string {
  const res = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", "Get-Clipboard -Raw"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (res.exitCode !== 0) {
    throw new Error(`issue-6008: Get-Clipboard failed: ${res.stderr.toString()}`);
  }
  return res.stdout.toString();
}

function requireDump(dump: string, needle: string) {
  if (!dump.includes(needle)) {
    throw new Error(`issue-6008: clipboard dump missing '${needle}':\n${dump}`);
  }
}

async function waitClipboardReplaced(sentinel: string): Promise<string> {
  const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
  let dump = clipboardText();
  while (dump.includes(sentinel) && Date.now() < deadline) {
    await sleep(50);
    dump = clipboardText();
  }
  return dump;
}

export async function testit(): Promise<void> {
  const sentinel = "issue-6008 clipboard sentinel";
  const set = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", `Set-Clipboard -Value '${sentinel}'`], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (set.exitCode !== 0) {
    throw new Error(`issue-6008: Set-Clipboard failed: ${set.stderr.toString()}`);
  }

  const { proc, client, frame } = await launchControlled([]);
  try {
    sendCommandSync(frame, cmdId("CmdHelpAbout"));
    const about = await waitForTopWindow(proc.pid!, ABOUT_CLASS, 8000 * SLOW_BUILD_FACTOR);
    if (!about) {
      throw new Error("issue-6008: About window did not open");
    }
    if (!getWindowText(about).includes("About")) {
      throw new Error(`issue-6008: unexpected About title '${getWindowText(about)}'`);
    }

    sendCommandSync(about, cmdId("CmdCopySelection"));
    let dump = await waitClipboardReplaced(sentinel);
    if (dump.includes(sentinel)) {
      throw new Error("issue-6008: Ctrl+C / CmdCopySelection did not replace the clipboard");
    }
    requireDump(dump, "SumatraPDF");
    requireDump(dump, "Built on:");
    requireDump(dump, "OS:");
    requireDump(dump, "OS architecture:");
    requireDump(dump, "WebView2:");
    requireDump(dump, "Physical memory:");
    if (dump.includes("Built: ")) {
      throw new Error(`issue-6008: dump still has 'Built: ' prefix:\n${dump}`);
    }
    const lines = dump.split(/\r?\n/).filter((l) => l.length > 0);
    if (lines.length < 6) {
      throw new Error(`issue-6008: expected a multi-line dump, got ${lines.length} lines:\n${dump}`);
    }

    const sentinel2 = "issue-6008 button sentinel";
    const set2 = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", `Set-Clipboard -Value '${sentinel2}'`], {
      stdout: "pipe",
      stderr: "pipe",
    });
    if (set2.exitCode !== 0) {
      throw new Error(`issue-6008: Set-Clipboard (button) failed: ${set2.stderr.toString()}`);
    }
    await waitForWindowIdle(about);
    const cr = getClientRect(about);
    // the copy-info button sits in the bottom padding, centered
    await clickAt(about, Math.floor(cr.right / 2), cr.bottom - 24, 400 * SLOW_BUILD_FACTOR);
    dump = await waitClipboardReplaced(sentinel2);
    if (dump.includes(sentinel2)) {
      throw new Error("issue-6008: button click did not copy program/machine info");
    }
    requireDump(dump, "SumatraPDF");
    requireDump(dump, "Built on:");
    requireDump(dump, "Physical memory:");
    if (getWindowPid(about) !== proc.pid) {
      throw new Error("issue-6008: About window vanished after click");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
