// Windows ending the session (logoff, shutdown) sends WM_QUERYENDSESSION, then
// WM_ENDSESSION: with no print job the app allows it, saves its settings (the
// open document goes into the history) and closes.
//
// Run: bun tests/end-session-saves.ts [-ng] [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { makeBlankPdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendMessage, sleep } from "./winapi.ts";

const WM_QUERYENDSESSION = 0x0011;
const WM_ENDSESSION = 0x0016;
const kExitWaitMs = 10000;

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP end-session-saves: a Windows session message");
    return;
  }
  const dir = tmpPath("end-session-saves");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const settings = join(dir, "SumatraPDF-settings.txt");
  writeFileSync(settings, "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n");
  const pdf = join(dir, "end-session.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    const allowed = sendMessage(frame, WM_QUERYENDSESSION, 0, 0);
    if (allowed !== 1n) {
      throw new Error(`end-session-saves: WM_QUERYENDSESSION returned ${allowed}, want 1`);
    }
    sendMessage(frame, WM_ENDSESSION, 1, 0);
    const deadline = Date.now() + kExitWaitMs;
    while (proc.exitCode === null && Date.now() < deadline) {
      await sleep(50);
    }
    if (proc.exitCode === null) {
      throw new Error("end-session-saves: app did not close on WM_ENDSESSION");
    }
    const saved = existsSync(settings) ? readFileSync(settings, "utf8") : "";
    if (!saved.includes("end-session.pdf")) {
      throw new Error(`end-session-saves: the open document is not in the saved settings:\n${saved}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
