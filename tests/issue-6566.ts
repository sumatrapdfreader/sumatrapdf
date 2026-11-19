// #6566: issue-6566.rtf has metafile pictures whose DIB headers claim more
// palette than the DIB holds. DibBitsOffset trusted it, and joining the tiles
// compared memory past the end of the picture: a crash. It must open (or fail
// to) without crashing.
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { launchControlled, killAndWait } from "./win-automation.ts";
import { runStandalone } from "./util.ts";
import { sleep } from "./winapi.ts";

export async function testit(): Promise<void> {
  const rtf = join(import.meta.dir, "issue-6566.rtf");
  const { proc, client } = await launchControlled([rtf]);
  try {
    await client.waitForRenderIdle(60_000).catch(() => {});
    await sleep(1_000);
    if (proc.exitCode !== null) {
      throw new Error(`issue-6566: SumatraPDF exited with ${proc.exitCode} opening the file`);
    }
    await client.request(ControlCommand.Ping, []);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
