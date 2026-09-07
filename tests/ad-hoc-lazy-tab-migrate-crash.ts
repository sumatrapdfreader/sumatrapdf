// Regression test for a use-after-free in LoadDocumentFinish.
//
// A lazily restored tab borrows its TabState from gInitialSessionData. When the
// legacy flat-page-number migration ran inside ReplaceDocumentInCurrentTab it
// called SaveSettings(), which frees and rebuilds gInitialSessionData. The tab
// being loaded already had a ctrl, so RefreshLazyTabStatePointers skipped it
// and SetTabState() then read the freed TabState (crash in Vec<int>::operator=).
//
// Scenario: session with two lazy tabs [pdf, epub], pdf selected, epub's
// FileState still has a flat PageNo. Closing the pdf tab loads the epub
// neighbor, which triggers the migration mid-load.
//
// Uses a scratch -appdata dir and no -for-testing so the session is restored.
//
// Run: bun tests/ad-hoc-lazy-tab-migrate-crash.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommandSync } from "./win-automation.ts";

const PDF = resolve("tests/issue-1189.pdf");
const EPUB = resolve("tests/issue-6095.epub");

const APPDATA = tmpPath("ad-hoc-lazy-tab-migrate-appdata");
const SETTINGS_PATH = join(APPDATA, "SumatraPDF-settings.txt");

// flat page number that MigrateFileStatePagePos turns into a "bm:" bookmark
const LEGACY_EPUB_PAGE_NO = 2;

function seedSettings(): void {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
  const tabState = (path: string, pageNo: number) => `      [
        FilePath = ${path}
        DisplayMode = single page
        PageNo = ${pageNo}
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]`;
  const seed = `RestoreSession = true
LazyLoading = true
UseTabs = true
RememberOpenedFiles = true
RememberStatePerDocument = true
FileStates [
  [
    FilePath = ${EPUB}
    UseDefaultState = false
    PageNo = ${LEGACY_EPUB_PAGE_NO}
  ]
  [
    FilePath = ${PDF}
    UseDefaultState = false
    PageNo = 1
  ]
]
SessionData [
  [
    TabStates [
${tabState(PDF, 1)}
${tabState(EPUB, LEGACY_EPUB_PAGE_NO)}
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`;
  writeFileSync(SETTINGS_PATH, seed, "utf8");
}

// on the unfixed code a debug build reads the freed TabState and drowns in
// debug reports instead of crashing, so the control pipe just goes silent
const STEP_TIMEOUT_MS = 60_000;

async function withDeadline<T>(label: string, p: Promise<T>): Promise<T> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  const timeout = new Promise<never>((_, reject) => {
    timer = setTimeout(
      () => reject(new Error(`${label}: no reply within ${STEP_TIMEOUT_MS} ms (app hung?)`)),
      STEP_TIMEOUT_MS,
    );
  });
  try {
    return await Promise.race([p, timeout]);
  } finally {
    clearTimeout(timer);
  }
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  seedSettings();
  const { proc, client, frame } = await launchControlled(["-appdata", APPDATA], { saveSettings: true });
  try {
    await withDeadline("initial render", client.waitForRenderIdle());

    // close the selected pdf tab: the lazy epub neighbor gets loaded now
    sendCommandSync(frame, cmdId("CmdClose"));
    await withDeadline("render after close", client.waitForRenderIdle());

    const info = await withDeadline("chapterInfo", client.chapterInfo());
    if (!info.hasChapters) {
      throw new Error(`expected the epub to be the front document, got: ${JSON.stringify(info)}`);
    }
    await withDeadline("quit", client.quit());
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await withDeadline("exit", proc.exited);
    if (exitCode !== 0) {
      throw new Error(`process exit code ${exitCode}, want 0`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
  }
  rmSync(APPDATA, { recursive: true, force: true });
  console.log("ad-hoc-lazy-tab-migrate-crash: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
