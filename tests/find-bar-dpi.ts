// Compact find bar must follow DPI in place. RecreateFindBar on WM_DPICHANGED
// heap-corrupted when a nested 96/120 change arrived during DestroyWindow
// (DameWare / RDP).
import { join } from "node:path";
import {
  cmdId,
  dpiRequest,
  parseDpiFields,
  requireDpiShrank,
  ROOT,
  runStandalone,
  waitForDpiFrame,
  writeAppdata,
} from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const dir = writeAppdata(
    "find-bar-dpi",
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nSearchUIFloating = false\n",
  );

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdFindFirst"));

    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // system -> 125%
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 125% -> 150%
    const high = await waitForDpiFrame(client, 144, (s) => (s.find ?? 0) > 0);

    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 150% -> 75%
    const low = await waitForDpiFrame(client, 72, (s) => (s.find ?? 0) > 0);
    requireDpiShrank("compact Find bar", high.fields.find ?? 0, low.fields.find ?? 0);
    requireDpiShrank("compact Find bar window", high.fields.findBarDy ?? 0, low.fields.findBarDy ?? 0);

    // Oscillate the way remote-desktop DPI storms do: many WM_DPICHANGED
    // deliveries, including nested ones from the find bar's own popup.
    for (let i = 0; i < 12; i++) {
      sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride"));
    }
    const afterRaw = await dpiRequest(client, "state");
    const after = parseDpiFields(afterRaw);
    if ((after.find ?? 0) <= 0) {
      throw new Error(`find-bar-dpi: compact find bar gone after DPI oscillation: ${afterRaw.trim()}`);
    }
    console.log("find-bar-dpi: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
