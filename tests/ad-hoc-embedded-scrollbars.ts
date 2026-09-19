// A host that embeds a normal SumatraPDF window (reparents it as WS_CHILD, like
// Total Commander's lister) gets native scrollbars at runtime. That override
// must not be written back to the settings file: before the fix, one embedded
// run turned the user's Scrollbars = smart into windows.
//
// A second SumatraPDF instance serves as the host; only its frame hwnd is used.
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { embedWindow } from "./winapi.ts";

const PDF = join(import.meta.dir, "issue-1189.pdf");

function scrollbarsIn(settings: string): string {
  const m = settings.match(/^Scrollbars = (\S+)/m);
  return m ? m[1] : "(missing)";
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata("ad-hoc-embedded-scrollbars-appdata", "Scrollbars = smart");
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");

  const host = await launchControlled([PDF]);
  try {
    // no -for-testing: the run has to save settings
    const guest = await launchControlled(["-appdata", appdata, PDF], { saveSettings: true });
    try {
      embedWindow(guest.frame, host.frame);
      // pumps messages, so the frame proc sees WS_CHILD and flips to embedded mode
      await guest.client.waitForRenderIdle();
      await guest.client.quit();
      await guest.proc.exited;
    } finally {
      await killAndWait(guest.proc);
    }
  } finally {
    await host.client.quit().catch(() => {});
    await killAndWait(host.proc);
  }

  const settings = readFileSync(settingsPath, "utf8");
  if (settings.split("\n").length < 10) {
    throw new Error("embedded run did not save settings; the test proves nothing");
  }
  const got = scrollbarsIn(settings);
  if (got !== "smart") {
    throw new Error(`embedded run rewrote Scrollbars to '${got}', expected 'smart'`);
  }
  console.log("ok: embedded run kept Scrollbars = smart");
}

if (import.meta.main) {
  await runStandalone(testit);
}
