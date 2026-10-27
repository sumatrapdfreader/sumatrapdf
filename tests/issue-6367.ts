// Home page UI issues from #6367:
// - the Home tab's window title (and taskbar button) was empty
//
// Run: bun tests/issue-6367.ts [--no-build]

import { writeFileSync } from "node:fs";
import { cmdId, makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { getWindowText } from "./winapi.ts";
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

export async function testit(): Promise<void> {
  await testHomeTitle();
}

if (import.meta.main) {
  await runStandalone(testit);
}
