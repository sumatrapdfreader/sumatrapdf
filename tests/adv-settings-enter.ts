// Native ng: Enter on a bool in Advanced Settings toggles it once. The filter field
// has the focus, so it reports Enter and the key then goes on to the window's handler;
// both ran the row, which toggled it back. tests/ad-hoc-wasm-enter.ts covers wasm.
//
// Run: bun tests/adv-settings-enter.ts -ng [--no-build]

import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, pollUntil, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand, waitToolWindow } from "./win-automation.ts";
import { postMessage, sleep, VK_DOWN, VK_RETURN, WM_KEYDOWN } from "./winapi.ts";

// ShowFileNavigateHint, DefaultDisplayMode, DefaultZoom, DisableJavaScript
const DOWNS_TO_BOOL = 4;
// long enough for a second, wrong toggle to have happened
const SETTLE_MS = 300;

async function nChanged(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestAdvSettingsRows, ["changed"]);
  return /changed=\d+/.exec(String(res[1] ?? ""))?.[0] ?? "";
}

export async function testit(): Promise<void> {
  // the frame's dialog is wasm's, and macOS can't post keys
  if (!USE_NG || IS_MAC) {
    console.log("SKIP adv-settings-enter: native ng's tool window");
    return;
  }
  const { proc, client, frame } = await launchControlled([]);
  try {
    sendCommand(frame, cmdId("CmdAdvancedSettings"));
    const tw = await waitToolWindow(client, proc.pid!, "advsettings");
    for (let i = 0; i < DOWNS_TO_BOOL; i++) {
      postMessage(tw, WM_KEYDOWN, VK_DOWN, 0);
    }
    const step = async (what: string, want: string) => {
      postMessage(tw, WM_KEYDOWN, VK_RETURN, 0);
      await pollUntil(
        () => nChanged(client),
        (s) => s === want,
        { error: (s) => `adv-settings-enter: ${what}: ${s}, want ${want}` },
      );
      await sleep(SETTLE_MS);
      const after = await nChanged(client);
      if (after !== want) {
        throw new Error(`adv-settings-enter: ${what} didn't last: ${after}, want ${want}`);
      }
    };
    await step("Enter didn't toggle the bool", "changed=1");
    await step("a second Enter didn't toggle it back", "changed=0");
    console.log("adv-settings-enter: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
