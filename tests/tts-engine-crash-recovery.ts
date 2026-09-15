// A SAPI voice engine (RHVoice, crash 2026-09-15-11-11-b6e9) crashing on a thread
// it started ends only that thread: the app keeps running, read aloud turns SAPI
// off and forgets the voice. withControlledSumatra fails on a non-zero exit.

import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, IS_ASAN, pollUntil, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";

async function engineCrash(client: ControlClient, action: string): Promise<string> {
  const res = await client.request(ControlCommand.TestTtsEngineCrash, [action]);
  const text = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`tts-engine-crash-recovery: ${action} failed: ${text}`);
  }
  return text;
}

export async function testit(): Promise<void> {
  // ASan reports the fake engine crash and aborts the process before any
  // handler of ours runs, so only a non-ASan build can test this
  if (IS_ASAN) {
    console.log("tts-engine-crash-recovery: skipped, an ASan build ends the process on any crash");
    return;
  }
  await withControlledSumatra(EXE, async (client) => {
    await engineCrash(client, "crash");
    await pollUntil(
      () => engineCrash(client, "state"),
      (s) => s === "crashed=1 voice=''",
      {
        timeoutMs: 10000 * SLOW_BUILD_FACTOR,
        error: (s) => `tts-engine-crash-recovery: expected crashed=1 voice='', got ${s}`,
      },
    );
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
