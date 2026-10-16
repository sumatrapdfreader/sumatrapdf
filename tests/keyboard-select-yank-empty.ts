// 'y' in keyboard text selection with only a caret (no selection) must not
// crash copying a missing selection.
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, cmdId, makeTextPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import { FRAME_CLASS, sendCommandSync } from "./win-automation";
import { postChar, sleep, waitForTopWindow } from "./winapi";

async function selectActive(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestSelectTextKeyboard, []);
  return /active=1/.test(String(res[1] ?? ""));
}

async function waitActive(client: ControlClient, want: boolean): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  while (Date.now() < deadline) {
    if ((await selectActive(client)) === want) {
      return;
    }
    await sleep(25);
  }
  throw new Error(`keyboard selection active should be ${want}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("keyboard-select-yank-empty.pdf");
  writeFileSync(pdf, makeTextPdf("The quick brown fox jumps over the lazy dog"));

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();

      sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
      await waitActive(client, true);

      await postChar(frame, "y");
      await waitActive(client, false);
    },
    [pdf],
  );
  console.log("keyboard-select-yank-empty: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
