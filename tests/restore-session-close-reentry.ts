// Restoring a session moves each window to its virtual desktop. That is a
// cross-process COM call, which dispatches incoming sent messages, so a DDE
// [CmdExit] arriving then closed the window and saved settings while WinMain
// was still using the window and the session data (crash 2026-10-20-10-29-433b).
//
// Run: bun tests/restore-session-close-reentry.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { assemblePdf, EXE, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, killAndWait, killProcessesNamed } from "./win-automation.ts";
import { findTopWindow, sendCopyDataW, sleep } from "./winapi.ts";

const kCopyDataDdeW = 0x44646557;
const kNumWindows = 3;
// any id works: the COM call is made (and fails) for a desktop that doesn't exist
const kDesktopId = "{6E3E6B1C-0D3B-4B8E-9C1A-2B7E5E2D9F11}";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function seedSettings(dir: string, pdf: string): void {
  let sessions = "";
  for (let i = 0; i < kNumWindows; i++) {
    sessions += `  [
    TabStates [
      [
        FilePath = ${pdf}
        DisplayMode = automatic
        PageNo = 1
        Zoom = fit page
      ]
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = ${100 + i * 20} 100 800 600
    VirtualDesktopId = ${kDesktopId}
  ]
`;
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = true
RememberOpenedFiles = true
ReuseInstance = false
UseTabs = true
LazyLoading = false
ShowStartPage = false
SessionData [
${sessions}]
`,
  );
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP restore-session-close-reentry: virtual desktops are a Windows shell feature");
    return;
  }
  await killProcessesNamed("SumatraPDF.exe");
  const dir = tmpPath("restore-session-close-reentry");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "page.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  seedSettings(dir, pdf);
  const log = join(dir, "log.txt");

  const proc = Bun.spawn([EXE, "-appdata", dir, "-log-to-file", log], { stdout: "ignore", stderr: "pipe" });
  try {
    // the first window is created hidden; WM_COPYDATA is dispatched at the
    // first point the UI thread accepts sent messages
    let frame = 0;
    const deadline = Date.now() + 15000;
    while (!frame && Date.now() < deadline) {
      frame = findTopWindow(proc.pid, FRAME_CLASS);
    }
    if (!frame) {
      throw new Error("restore-session-close-reentry: no frame window");
    }
    sendCopyDataW(frame, kCopyDataDdeW, "[CmdExit]");

    const exited = await Promise.race([proc.exited, sleep(20000).then(() => -1)]);
    if (exited === -1) {
      throw new Error("restore-session-close-reentry: app did not exit");
    }
    if (exited !== 0) {
      const tail = readFileSync(log, "utf8").split("\n").slice(-15).join("\n");
      throw new Error(`restore-session-close-reentry: exit code ${exited}, want 0\n${tail}`);
    }
  } finally {
    await killAndWait(proc);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
