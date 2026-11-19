// #6540: CmdSelectTextViaKeyboard takes the toggle commands' state argument, so
// [CmdSelectTextViaKeyboard on] turns keyboard selection (F7) on without
// turning it off when it already is.
import { writeFileSync } from "node:fs";
import type { ControlClient } from "./control";
import { withControlledSumatra } from "./control";
import { getState, makeTextPdf, waitForState } from "./issue-4684";
import { EXE, runStandalone, tmpPath } from "./util";
import { FRAME_CLASS } from "./win-automation";
import { sendCopyDataW, sleep, waitForTopWindow } from "./winapi";

const kCopyDataDdeW = 0x44646557;

async function dde(client: ControlClient, frame: number, cmd: string): Promise<void> {
  if (!sendCopyDataW(frame, kCopyDataDdeW, cmd)) throw new Error(`issue-6540: DDE ${cmd} not handled`);
  await client.waitForRenderIdle();
}

// the mode must stay `active` across a settle period, not just flicker into it
async function expectStays(client: ControlClient, active: boolean, what: string): Promise<void> {
  await waitForState(client, (s) => s.active === active);
  await sleep(200);
  const { state, dump } = await getState(client);
  if (state.active !== active) throw new Error(`issue-6540: ${what}: active=${state.active}\n${dump}`);
}

async function run(client: ControlClient, frame: number): Promise<void> {
  await client.waitForRenderIdle();

  await dde(client, frame, "[CmdSelectTextViaKeyboard on]");
  await expectStays(client, true, "first 'on'");
  await dde(client, frame, "[CmdSelectTextViaKeyboard on]");
  await expectStays(client, true, "'on' when already on");

  await dde(client, frame, "[CmdSelectTextViaKeyboard off]");
  await expectStays(client, false, "first 'off'");
  await dde(client, frame, "[CmdSelectTextViaKeyboard off]");
  await expectStays(client, false, "'off' when already off");

  // no state: still a toggle
  await dde(client, frame, "[CmdSelectTextViaKeyboard]");
  await expectStays(client, true, "plain toggle");
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6540.pdf");
  writeFileSync(pdf, makeTextPdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("issue-6540: no frame window");
      await run(client, frame);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
