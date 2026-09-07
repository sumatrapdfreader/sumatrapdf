// WM_QUERYENDSESSION handling: with no print job running the frame must
// allow logoff/shutdown (return TRUE) and stay alive afterwards. The
// blocking path (a print job in progress) needs a real printer, so it is
// not driven here.
//
// Run: bun tests/ad-hoc-query-end-session.ts [--no-build]

import { resolve } from "node:path";
import { runStandalone } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled } from "./win-automation.ts";
import { sendMessage } from "./winapi.ts";

const PDF = resolve("tests/issue-1189.pdf");
const WM_QUERYENDSESSION = 0x0011;

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();

    const res = sendMessage(frame, WM_QUERYENDSESSION, 0, 0);
    if (res !== 1n) {
      throw new Error(`WM_QUERYENDSESSION returned ${res}, want 1 (no print job running)`);
    }
    // the query alone must not close anything
    await client.waitForRenderIdle();
    await client.quit();
  } catch (e) {
    await killAndWait(proc);
    throw e;
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
  }
  console.log("ad-hoc-query-end-session: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
