// #5979: UI created or retained on a high-DPI monitor must use the new
// monitor's fonts after the frame moves to a lower-DPI monitor.
import { join } from "node:path";
import { cmdId, dpiRequest, requireDpiShrank, ROOT, runStandalone, waitForDpiFrame, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

function toggleTo150(frame: number): void {
  sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // system -> 125%
  sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 125% -> 150%
}

function toggleTo75(frame: number): void {
  sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 150% -> 75%
}

export async function testit(): Promise<void> {
  const dir = writeAppdata(
    "issue-5979",
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowToc = true\nSearchUIFloating = true\n",
  );

  const home = await launchControlled(["-appdata", dir]);
  try {
    await dpiRequest(home.client, "hidden");
    toggleTo150(home.frame);
    const high = await waitForDpiFrame(home.client, 144, (s) => (s.home ?? 0) > 0 && (s.tocEdit ?? 0) > 0);
    toggleTo75(home.frame);
    const low = await waitForDpiFrame(home.client, 72, (s) => (s.home ?? 0) > 0 && (s.tocEdit ?? 0) > 0);

    requireDpiShrank("Home search", high.fields.home ?? 0, low.fields.home ?? 0);
    requireDpiShrank("Bookmarks label", high.fields.tocLabel ?? 0, low.fields.tocLabel ?? 0);
    requireDpiShrank("Bookmarks search", high.fields.tocEdit ?? 0, low.fields.tocEdit ?? 0);
  } finally {
    home.client.close();
    await killAndWait(home.proc);
  }

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const find = await launchControlled(["-appdata", dir, pdf]);
  try {
    await find.client.waitForRenderIdle();
    toggleTo150(find.frame);
    sendCommandSync(find.frame, cmdId("CmdFindFirst"));
    // This creates the panel when Claude and WebView2 are available; the DPI
    // assertions below stay optional so hosted runners without them can pass.
    sendCommandSync(find.frame, cmdId("CmdAIChatWithClaudeCode"));
    const high = await waitForDpiFrame(find.client, 144, (s) => (s.find ?? 0) > 0);
    toggleTo75(find.frame);
    const low = await waitForDpiFrame(find.client, 72, (s) => (s.find ?? 0) > 0);
    requireDpiShrank("floating Find", high.fields.find ?? 0, low.fields.find ?? 0);
    if ((high.fields.aiInput ?? 0) > 0 || (low.fields.aiInput ?? 0) > 0) {
      requireDpiShrank("AI chat label", high.fields.aiLabel ?? 0, low.fields.aiLabel ?? 0);
      requireDpiShrank("AI chat input", high.fields.aiInput ?? 0, low.fields.aiInput ?? 0);
      requireDpiShrank("AI chat checkbox", high.fields.aiCheckbox ?? 0, low.fields.aiCheckbox ?? 0);
    }
  } finally {
    find.client.close();
    await killAndWait(find.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
