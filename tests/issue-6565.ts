// #6565: issue-6565.lit has LZX code lengths that make the Huffman table
// builder (ext/chmdec make_decode_table) follow a symbol entry as a tree node
// and write past the table. Opening it crashed the ASan build in the
// thumbnail thread; other builds silently corrupted memory. It must open (or
// fail to) without crashing.
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { launchControlled, killAndWait } from "./win-automation.ts";
import { runStandalone } from "./util.ts";
import { sleep } from "./winapi.ts";

export async function testit(): Promise<void> {
  const lit = join(import.meta.dir, "issue-6565.lit");
  const { proc, client } = await launchControlled([lit]);
  try {
    await client.waitForRenderIdle(60_000).catch(() => {});
    // the thumbnail for the home page is made on a background thread
    await sleep(2_000);
    if (proc.exitCode !== null) {
      throw new Error(`issue-6565: SumatraPDF exited with ${proc.exitCode} opening the file`);
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
