// Debug report `!gPluginMode` in MainWindow::IsDocLoaded (crash 2026-09-18-00-46-d287).
//
// Selecting a lazily restored tab goes through LoadModelIntoTab, which shows a
// "loading" notification over the outgoing document and calls ShowMainWindow,
// whose UpdateWindow paints the canvas synchronously. The tab bar has already
// selected the new tab, so CurrentTab() has no ctrl while win->ctrl is still the
// outgoing document; the paint's IsDocLoaded() sees the mismatch and reports.
//
// Uses a scratch -appdata dir and no -for-testing, so the session is restored.
// Without -for-testing a debug report does not end the process, it only writes
// the report to stderr, so the test checks stderr.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommand, takeStderr } from "./win-automation.ts";

const LAZY_PDF = resolve("tests/issue-1189.pdf");
const LOADED_PDF = resolve("tests/issue-1809.pdf");

const APPDATA = tmpPath("lazy-tab-select-paint-appdata");

function seedSettings(): void {
  rmSync(APPDATA, { recursive: true, force: true });
  mkdirSync(APPDATA, { recursive: true });
  const tabState = (path: string) => `      [
        FilePath = ${path}
        DisplayMode = single page
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]`;
  const seed = `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
LazyLoading = true
UseTabs = true
HighlightFormFields = true
RememberOpenedFiles = true
RememberStatePerDocument = true
ReuseInstance = false
SessionData [
  [
    TabStates [
${tabState(LAZY_PDF)}
${tabState(LOADED_PDF)}
    ]
    TabIndex = 2
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`;
  writeFileSync(join(APPDATA, "SumatraPDF-settings.txt"), seed, "utf8");
}

async function currentPath(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const raw = String(res[1] ?? "").trim();
  const m = /^path=(.+?) page=/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`lazy-tab-select-paint: could not read current tab: ${raw}`);
  }
  return resolve(m[1]!);
}

async function placementActive(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`lazy-tab-select-paint: could not read placement state: ${raw.trim()}`);
  }
  return /Placement active=1/.test(raw);
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  seedSettings();
  const { proc, client, frame } = await launchControlled(["-appdata", APPDATA], { saveSettings: true });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);

    // an active placement is cancelled while closing the outgoing document,
    // which refreshes the toolbar through IsDocLoaded (crash 2026-09-19-12-22-8c3a)
    sendCommand(frame, cmdId("CmdCreateAnnotSquare"));
    await pollUntil(
      () => placementActive(client),
      (active) => active,
      {
        timeoutMs: 5000 * SLOW_BUILD_FACTOR,
        error: "lazy-tab-select-paint: square placement did not start",
      },
    );

    // selecting the lazy tab paints the outgoing document under a notification
    // posted: SendMessage would deadlock with the app writing the report to our stderr
    sendCommand(frame, cmdId("CmdPrevTab"));
    await pollUntil(
      () => currentPath(client),
      (path) => path === LAZY_PDF,
      {
        timeoutMs: 5000 * SLOW_BUILD_FACTOR,
        error: (path) => `lazy-tab-select-paint: selected ${path}, want ${LAZY_PDF}`,
      },
    );
    await client.waitForRenderIdle(30000);

    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  try {
    const exitCode = await proc.exited;
    const stderr = await takeStderr(proc);
    if (stderr.includes("MainWindow.cpp") || stderr.includes("IsDocLoaded")) {
      throw new Error(`lazy-tab-select-paint: debug report fired:\n${stderr}`);
    }
    if (exitCode !== 0) {
      throw new Error(`lazy-tab-select-paint: exit code ${exitCode}, want 0`);
    }
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
    rmSync(APPDATA, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
