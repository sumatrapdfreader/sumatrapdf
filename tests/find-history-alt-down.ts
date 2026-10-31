// Alt+Down in the find field drops the find history list, Alt+Up closes it.
// The find bar is an owned popup, whose system keys go to the frame so that
// Alt+<mnemonic> opens the menu; these two have to stay with the combo.
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control";
import { ROOT, USE_NG, cmdId, runStandalone } from "./util";
import { getParentWindow, postMessage, sendMessage, sleep, VK_DOWN, VK_UP } from "./winapi";
import {
  killAndWait,
  launchControlled,
  ngFind,
  pressKey,
  sendCommand,
  typeIntoInput,
  waitFindOpen,
  waitForFocusClass,
} from "./win-automation";

const VK_RETURN = 0x0d;
const WM_SYSKEYDOWN = 0x0104;
const CB_GETDROPPEDSTATE = 0x0157;
// lParam bit 29: Alt is down
const ALT_DOWN_LPARAM = 1 << 29;

async function waitDropped(combo: number, want: boolean, what: string): Promise<void> {
  const deadline = Date.now() + 3000;
  while (Date.now() < deadline) {
    if ((Number(sendMessage(combo, CB_GETDROPPEDSTATE, 0, 0)) !== 0) === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-history-alt-down: ${what}: history list dropped should be ${want}`);
}

async function waitHistoryOpen(client: ControlClient, what: string): Promise<void> {
  const deadline = Date.now() + 5000;
  let last = "";
  while (Date.now() < deadline) {
    last = String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "");
    if (last.includes("historyOpen=1")) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`find-history-alt-down: ${what}: history list didn't open: ${last.trim()}`);
}

// CmdFindHistory, Alt+Down outside the find field: opens find and its history
async function testCommand(pdf: string): Promise<void> {
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    // nothing to show yet
    sendCommand(frame, cmdId("CmdFindHistory"));
    if (USE_NG) {
      await waitFindOpen(client);
      await ngFind(client, "set", "deflate");
      await ngFind(client, "enter");
    } else {
      const edit = await waitForFocusClass(frame, "Edit");
      await typeIntoInput(getParentWindow(edit), "deflate", false);
      await pressKey(edit, VK_RETURN);
    }
    await sleep(300);

    sendCommand(frame, cmdId("CmdFindHistory"));
    await waitHistoryOpen(client, "find bar");

    // the floating window, as the user may have chosen
    await client.request(ControlCommand.TestFindUiState, ["toggle-first"]);
    await sleep(500);
    sendCommand(frame, cmdId("CmdFindHistory"));
    await waitHistoryOpen(client, "find window");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  await testCommand(pdf);
  if (USE_NG) {
    // gpui reads Alt from the keyboard state, which a posted message doesn't set
    console.log("find-history-alt-down: OK (the keys are orig only)");
    return;
  }
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit = await waitForFocusClass(frame, "Edit");
    const combo = getParentWindow(edit);
    await typeIntoInput(combo, "deflate", false);
    await pressKey(edit, VK_RETURN);

    postMessage(edit, WM_SYSKEYDOWN, VK_DOWN, ALT_DOWN_LPARAM);
    await waitDropped(combo, true, "Alt+Down");
    postMessage(edit, WM_SYSKEYDOWN, VK_UP, ALT_DOWN_LPARAM);
    await waitDropped(combo, false, "Alt+Up");
    console.log("find-history-alt-down: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
