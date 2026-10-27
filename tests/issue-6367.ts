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

type R = number[];
const right = (r: R) => r[0]! + r[2]!;
const isEmpty = (r: R) => r[2]! <= 0 || r[3]! <= 0;
const overlapX = (a: R, b: R) => a[0]! < right(b) && b[0]! < right(a);

// the header row's items fit the window side by side, and the tip wraps at
// the row's width, not at one thumbnail's
function checkHeader(h: HomeSelection, w: number, nFiles: number): string {
  const items: [string, R][] = [
    ["logo", h.logo],
    ["search", h.search],
    ["open", h.open],
    ["views", h.views],
  ];
  const where = `${nFiles} files, window ${w}px: ${h.raw}`;
  for (const name of ["search", "open", "views"]) {
    if (isEmpty(items.find((it) => it[0] === name)![1])) {
      return `${name} hidden at ${where}`;
    }
  }
  const clientDx = h.tipRect[0]! + h.tipRect[2]!;
  for (let i = 0; i < items.length; i++) {
    const [na, a] = items[i]!;
    if (isEmpty(a)) {
      continue;
    }
    if (a[0]! < 0 || right(a) > clientDx) {
      return `${na} outside the window at ${where}`;
    }
    for (let j = i + 1; j < items.length; j++) {
      const [nb, b] = items[j]!;
      if (!isEmpty(b) && overlapX(a, b)) {
        return `${na} overlaps ${nb} at ${where}`;
      }
    }
  }
  const rowDx = right(h.views) - (isEmpty(h.logo) ? h.search[0]! : h.logo[0]!);
  if (h.tipText[2]! < rowDx) {
    return `tip wraps at ${h.tipText[2]}px, narrower than the ${rowDx}px header at ${where}`;
  }
  return "";
}

async function testHomeHeader(nFiles: number): Promise<void> {
  const dir = makeAppDir(`header-${nFiles}`, nFiles, "ShowTips = true\n");
  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    for (const w of [900, 558, 557, 450, 380]) {
      moveWindow(frame, 100, 100, w, 450);
      let last: HomeSelection | null = null;
      // wait for the layout of this width: the tip band spans the client area
      await waitHome(
        client,
        (h) => {
          last = h;
          return h.tipRect[2]! > w - 60 && h.tipRect[2]! <= w;
        },
        `no layout for ${w}px`,
      );
      // ng measures text while painting: give it a frame to settle
      await Bun.sleep(200);
      last = await client.homeSelection();
      const err = checkHeader(last, w, nFiles);
      if (err) {
        throw new Error(`issue-6367: ${err}`);
      }
    }
    await client.quit();
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await testHomeTitle();
  await testHomeScrollbar();
  await testHomeHeader(2);
  await testHomeHeader(0);
}

if (import.meta.main) {
  await runStandalone(testit);
}
