// Sign Document's new-signature placement belongs to the tab it started in.
// Switching to or closing that tab must end placement, or the next page click
// signs whatever document is current (round 5 review C15).
//
// Run: bun tests/sign-placement-tab-switch.ts [--no-build] [-ng]

import { copyFileSync, existsSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { writeTestPfx } from "./pki.ts";
import { ROOT, USE_NG, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { macSignUi, winSignUi, type SignUi } from "./ad-hoc-sign-document.ts";
import { postMessage, sleep, WM_CLOSE } from "./winapi.ts";

const kPassword = "sumatra-test-pw";
const kNotPlacing = 2;

function signUi(pid: number, client: ControlClient, frame: number): SignUi {
  if (!USE_NG) {
    return winSignUi(pid, frame, client);
  }
  const ui = macSignUi(client, frame);
  if (!IS_MAC) {
    ui.saveTo = winSignUi(pid, frame, client).saveTo;
  }
  return ui;
}

// 0 = placement took the click; kNotPlacing = no placement is active
async function placeResult(client: ControlClient, x = 100, y = 100): Promise<number> {
  const res = await client.request(ControlCommand.TestPlaceSignature, [x, y]);
  return Number(res[0]);
}

// an off-page click keeps placement waiting, so it probes without signing
async function expectPlacing(client: ControlClient): Promise<void> {
  if ((await placeResult(client, -10000, -10000)) !== 0) {
    throw new Error("Sign did not start signature placement");
  }
}

export async function testit(): Promise<void> {
  const pfx = tmpPath("sign-tab-switch.pfx");
  writeTestPfx(pfx, "SumatraPDF SignTest", kPassword);
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const a = tmpPath("sign-tab-a.pdf");
  const b = tmpPath("sign-tab-b.pdf");
  copyFileSync(src, a);
  copyFileSync(src, b);
  const out = tmpPath("sign-tab-out.pdf");
  rmSync(out, { force: true });

  const { proc, client, frame } = await launchControlled([a, b]);
  try {
    await client.waitForRenderIdle();
    const ui = signUi(proc.pid!, client, frame);
    await ui.saveTo(out);

    // switch: start placing in a, select b, click
    sendCommandSync(frame, cmdId("CmdPrevTab"));
    await client.waitForRenderIdle();
    await ui.sign(pfx, kPassword);
    await expectPlacing(client);
    sendCommandSync(frame, cmdId("CmdNextTab"));
    await client.waitForRenderIdle();
    const afterSwitch = await placeResult(client);
    await sleep(500);
    if (afterSwitch !== kNotPlacing || existsSync(out)) {
      throw new Error(`placement survived a tab switch (result ${afterSwitch}, signed: ${existsSync(out)})`);
    }

    // close: start placing in b, close it, click on a
    await ui.sign(pfx, kPassword);
    await expectPlacing(client);
    sendCommandSync(frame, cmdId("CmdClose"));
    await client.waitForRenderIdle();
    const afterClose = await placeResult(client);
    await sleep(500);
    if (afterClose !== kNotPlacing || existsSync(out)) {
      throw new Error(`placement survived closing its tab (result ${afterClose}, signed: ${existsSync(out)})`);
    }
  } finally {
    client.close();
    postMessage(frame, WM_CLOSE, 0, 0);
    await killAndWait(proc);
    rmSync(pfx, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
