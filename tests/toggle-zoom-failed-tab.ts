// Debug report `!ctrl` in WindowTab::ToggleZoom (crash 2026-09-27-02-15-eee0).
//
// A document that fails to load leaves a tab without a controller; pressing Z
// (CmdToggleZoom) on it must be a no-op. A debug report under -for-testing
// exits with code 105.

import { writeFileSync } from "node:fs";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

// parses but has no pages: "document has no pages"
const BROKEN_PDF = "%PDF-1.4\n1 0 obj\n<< /Type /Catalog >>\nendobj\ntrailer\n<< /Root 1 0 R >>\n%%EOF\n";

export async function testit(): Promise<void> {
  const path = tmpPath("toggle-zoom-failed-tab.pdf");
  writeFileSync(path, BROKEN_PDF, "latin1");

  const { proc, client, frame } = await launchControlled([path]);
  sendCommand(frame, cmdId("CmdToggleZoom"));
  await sleep(500 * SLOW_BUILD_FACTOR);

  // a debug report ends the process, so quit only if it is still running
  if (proc.exitCode === null) {
    try {
      await client.quit();
    } catch (e) {
      await killAndWait(proc);
      throw e;
    }
  }

  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`toggle-zoom-failed-tab: exit code ${exitCode}, want 0 (105 = debug report)`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
