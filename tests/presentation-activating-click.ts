// A click that brings an inactive presentation window to the front only
// activates it: orig answers WM_MOUSEACTIVATE with MA_ACTIVATEANDEAT, so
// Windows drops the click instead of turning the page. Outside presentation
// the click goes through (MA_ACTIVATE). The test sends the message Windows
// sends for a click and checks the answer, which is what Windows acts on.
//
// Run: bun tests/presentation-activating-click.ts [--no-build] [-ng]

import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { getForegroundWindow, sendMessage, sleep } from "./winapi.ts";

const name = "presentation-activating-click";
const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const WM_MOUSEACTIVATE = 0x0021;
const WM_LBUTTONDOWN = 0x0201;
const HTCLIENT = 1;
const MA_ACTIVATE = 1;
const MA_ACTIVATEANDEAT = 2;

function mouseActivate(frame: number): number {
  return Number(sendMessage(frame, WM_MOUSEACTIVATE, frame, (WM_LBUTTONDOWN << 16) | HTCLIENT));
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log(`SKIP ${name}: WM_MOUSEACTIVATE is Windows only`);
    return;
  }
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    if (getForegroundWindow() === frame) {
      console.log(`SKIP ${name}: the window is active, so there is no activating click`);
      return;
    }
    let res = mouseActivate(frame);
    if (res !== MA_ACTIVATE) {
      throw new Error(`${name}: outside presentation the click should go through: ${res}, want ${MA_ACTIVATE}`);
    }

    await client.request(ControlCommand.TestInvokeCommand, ["CmdTogglePresentationMode"]);
    await sleep(500);
    res = mouseActivate(frame);
    if (res !== MA_ACTIVATEANDEAT) {
      throw new Error(
        `${name}: in presentation the activating click should be eaten: ${res}, want ${MA_ACTIVATEANDEAT}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
