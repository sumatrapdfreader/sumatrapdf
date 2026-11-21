// The DDE request [GetMousePos] (#1411) answers with the page and the PDF point
// under the mouse cursor. ng used to answer "error: unknown command".
//
// Run: bun tests/dde-get-mouse-pos.ts [-ng] [--no-build]

import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled } from "./win-automation.ts";
import { ddeRequest } from "./winapi.ts";

const PDF = join(ROOT, "docs", "test", "zlib.3.pdf");

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP dde-get-mouse-pos: DDE is Windows only");
    return;
  }
  // DDEML talks to the first SUMATRA server that answers
  await killProcessesNamed("SumatraPDF.exe");
  const { proc, client } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    const res = ddeRequest("[GetMousePos]");
    if (!res || !/^page: \d+\nx: [\d.]+\ny: [\d.]+/.test(res.replace(/\r\n/g, "\n"))) {
      throw new Error(`dde-get-mouse-pos: unexpected answer ${JSON.stringify(res)}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
