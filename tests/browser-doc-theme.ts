// Markdown and CHM pages are html with the page colors baked in, so a theme
// change that changes the page colors (DocumentColorsFollowTheme) reloads
// them. ng used to keep showing the old colors.
//
// Run: bun tests/browser-doc-theme.ts [-ng] [--no-build]

import { existsSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { pollUntil, ROOT, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const MD = join(ROOT, "docs", "test", "test.md");
const kReloaded = "BrowserDocController::UpdateTheme: reloading";
const kToggleEveryMs = 500;

function reloads(log: string): number {
  const text = existsSync(log) ? readFileSync(log, "utf8") : "";
  return text.split(kReloaded).length - 1;
}

async function toggleTheme(client: ControlClient): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleLightDarkTheme"]);
  await sleep(kToggleEveryMs);
}

export async function testit(): Promise<void> {
  const log = tmpPath("browser-doc-theme.log");
  rmSync(log, { force: true });
  const appdata = writeAppdata(
    "browser-doc-theme",
    "UiLanguage = en\nCheckForUpdates = false\nDocumentColorsFollowTheme = smart\n",
  );
  const { proc, client } = await launchControlled(["-appdata", appdata, "-log-to-file", log, MD]);
  try {
    await client.waitForRenderIdle();
    // the page's browser view may still be starting: a change before it shows
    // the page has nothing to reload, so toggle until one does
    const first = await pollUntil(
      async () => {
        await toggleTheme(client);
        return reloads(log);
      },
      (n) => n > 0,
      { timeoutMs: 10000, error: "browser-doc-theme: the markdown page was not reloaded for the new theme" },
    );
    // and the next change too
    await toggleTheme(client);
    await pollUntil(
      async () => reloads(log),
      (n) => n > first,
      { timeoutMs: 3000, error: "browser-doc-theme: the second theme change didn't reload the page" },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
