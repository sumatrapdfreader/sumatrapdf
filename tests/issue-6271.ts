// A saved session records the virtual desktop each window was on, and
// restoring the session puts the window back on that desktop (#6271).
//
// Only the current desktop exists on a test machine, so the restore half is
// checked through the app's log: the move to the recorded desktop must succeed.
//
// Run: bun tests/issue-6271.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled } from "./win-automation.ts";
import { getWindowVirtualDesktopId } from "./winapi.ts";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function seedSettings(dir: string, pdf: string): void {
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
ReuseInstance = false
UseTabs = true
ShowStartPage = false
SessionData [
  [
    TabStates [
      [
        FilePath = ${pdf}
        DisplayMode = automatic
        PageNo = 1
        Zoom = fit page
        Rotation = 0
        ScrollPos = -1 -1
        ShowToc = false
      ]
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 100 100 800 600
  ]
]
`,
  );
}

// start with the session in dir, wait for it, run probe on the frame, quit
async function runSession(dir: string, log: string, probe: (frame: number) => void): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", dir, "-log-to-file", log], {
    saveSettings: true,
  });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);
    probe(frame);
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`issue-6271: exit code ${exitCode}, want 0`);
  }
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP issue-6271: virtual desktops are a Windows shell feature");
    return;
  }
  await killProcessesNamed("SumatraPDF.exe");
  const dir = tmpPath("issue-6271");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "page.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  seedSettings(dir, pdf);

  // save: the session must record the desktop the shell says the window is on
  let want = "";
  await runSession(dir, join(dir, "save.log"), (frame) => {
    want = getWindowVirtualDesktopId(frame);
  });
  if (!want) {
    console.log("issue-6271: IVirtualDesktopManager gives no desktop id in this session, skipping");
    rmSync(dir, { recursive: true, force: true });
    return;
  }
  const settings = readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
  const m = /^\s*VirtualDesktopId = (\S+)/m.exec(settings);
  const got = m ? m[1]! : "";
  if (got.toUpperCase() !== want.toUpperCase()) {
    throw new Error(`issue-6271: saved VirtualDesktopId is '${got}', want '${want}'`);
  }

  // restore: the window is moved to the recorded desktop
  const restoreLog = join(dir, "restore.log");
  await runSession(dir, restoreLog, () => {});
  const log = readFileSync(restoreLog, "utf8");
  const moved = new RegExp(`MoveWindowToVirtualDesktop: ok,.*desktop: ${want}`, "i");
  if (!moved.test(log)) {
    throw new Error(`issue-6271: restore did not move the window to ${want}`);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
