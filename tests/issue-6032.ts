// #6032: Bookmarks/Favorites header close button must follow the frame DPI
// (it stayed at the previous monitor's size after a DPI change).
import { cmdId, requireDpiShrank, runStandalone, waitForDpiFrame, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const dir = writeAppdata(
    "issue-6032",
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowToc = true\nShowFavorites = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // system -> 125%
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 125% -> 150%
    const high = await waitForDpiFrame(client, 144, (s) => (s.tocClose ?? 0) > 0 && (s.favClose ?? 0) > 0);
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 150% -> 75%
    const low = await waitForDpiFrame(client, 72, (s) => (s.tocClose ?? 0) > 0 && (s.favClose ?? 0) > 0);
    requireDpiShrank("Bookmarks close", high.fields.tocClose ?? 0, low.fields.tocClose ?? 0);
    requireDpiShrank("Favorites close", high.fields.favClose ?? 0, low.fields.favClose ?? 0);
    console.log("issue-6032: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
