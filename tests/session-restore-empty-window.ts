// Restoring a session whose middle window has no tabs: that window is closed
// mid-restore, which saves the settings and rebuilds the session list the
// restore loop is iterating. The windows after it must still be restored.
//
// Run: bun tests/session-restore-empty-window.ts [--no-build] -ng
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function windowWithTab(pdf: string, x: number): string {
  return `  [
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
    WindowPos = ${x} 100 600 500
  ]
`;
}

function seedSettings(dir: string, pdfA: string, pdfB: string): void {
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
${windowWithTab(pdfA, 100)}  [
    TabStates [
    ]
    TabIndex = 1
    WindowState = 1
    WindowPos = 200 100 600 500
  ]
${windowWithTab(pdfB, 300)}]
`,
  );
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    // orig keeps empty restored windows open and never saves mid-restore
    console.log("SKIP session-restore-empty-window: ng only");
    return;
  }
  const dir = tmpPath("session-restore-empty-window");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfA = join(dir, "a.pdf");
  const pdfB = join(dir, "b.pdf");
  writeFileSync(pdfA, makePdf(), "latin1");
  writeFileSync(pdfB, makePdf(), "latin1");
  seedSettings(dir, pdfA, pdfB);
  const logPath = join(dir, "restore.log");

  const { proc, client } = await launchControlled(["-appdata", dir, "-log-to-file", logPath], {
    saveSettings: true,
  });
  try {
    await client.waitForSessionRestored(30000);
    await client.waitForRenderIdle(30000);
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`session-restore-empty-window: exit code ${exitCode}, want 0`);
  }
  const log = readFileSync(logPath, "utf8");
  const m = /RestoreSession: restored (\d+) tabs in (\d+) windows/.exec(log);
  if (!m || m[1] !== "2" || m[2] !== "2") {
    throw new Error(`session-restore-empty-window: want 2 tabs in 2 windows, log says: ${m ? m[0] : "nothing"}`);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
