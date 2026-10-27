// Home page UI issues from #6367:
// - the Home tab's window title (and taskbar button) was empty
// - with the default (windows) scrollbars a long file list had no scrollbar
//
// Run: bun tests/issue-6367.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient, HomeSelection } from "./control.ts";
import { cmdId, makeTextPdf, pollUntil, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { getWindowText, moveWindow } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

async function testHomeTitle(): Promise<void> {
  const pdf = tmpPath("issue-6367.pdf");
  writeFileSync(pdf, makeTextPdf("issue 6367"));
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await pollUntil(
      () => getWindowText(frame),
      (t) => t.includes("issue-6367.pdf"),
      { error: (t) => `issue-6367: document title missing: '${t}'` },
    );
    sendCommand(frame, cmdId("CmdGoToHomePage"));
    const want = "Home - SumatraPDF";
    await pollUntil(
      () => getWindowText(frame),
      (t) => t === want,
      { timeoutMs: 3000, error: (t) => `issue-6367: Home tab title is '${t}', want '${want}'` },
    );
    await client.quit();
  } finally {
    await killAndWait(proc);
  }
}

// an app data dir whose history has nFiles documents
function makeAppDir(name: string, nFiles: number, extra = ""): string {
  const dir = tmpPath(`issue-6367-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "docs"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "docs", `doc-${String(i).padStart(2, "0")}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\n${extra}FileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

async function waitHome(client: ControlClient, ok: (h: HomeSelection) => boolean, what: string) {
  return pollUntil(
    () => client.homeSelection(),
    (h) => h.ready && ok(h),
    { error: (h) => `issue-6367: ${what}: ${h.raw}` },
  );
}

async function testHomeScrollbar(): Promise<void> {
  // ng has no windows mode; its smart bar would auto-hide
  const dir = makeAppDir("scroll", 20, USE_NG ? "Scrollbars = overlay\n" : "");
  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    moveWindow(frame, 100, 100, 700, 450);
    await waitHome(client, (h) => h.entries === 20 && h.vbar, "no scrollbar for 20 files in a small window");
    // tall enough for every entry: no bar
    moveWindow(frame, 0, 0, 1900, 1400);
    await waitHome(client, (h) => !h.vbar, "scrollbar shown with nothing to scroll");
    await client.quit();
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await testHomeTitle();
  await testHomeScrollbar();
}

if (import.meta.main) {
  await runStandalone(testit);
}
