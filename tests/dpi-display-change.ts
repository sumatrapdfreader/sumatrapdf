// An RDP connect can change the session DPI with no WM_DPICHANGED, only a
// WM_DISPLAYCHANGE (#4581). Orig re-reads the window DPI then and re-lays out
// for it. TestDpi "override" makes the window report another DPI silently.
//
// Run: bun tests/dpi-display-change.ts [--no-build] [-ng]

import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { dpiRequest, parseDpiFields, ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendMessage, sleep } from "./winapi.ts";

const name = "dpi-display-change";
const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const WM_DISPLAYCHANGE = 0x007e;

type Client = Parameters<typeof dpiRequest>[0];

async function frameDpi(client: Client): Promise<number> {
  return parseDpiFields(await dpiRequest(client, "state")).frame ?? 0;
}

async function waitFrameDpi(client: Client, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 3000;
  let got = 0;
  while (Date.now() < deadline) {
    got = await frameDpi(client);
    if (got === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`${name}: ${what}: frame DPI ${got}, want ${want}`);
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log(`SKIP ${name}: WM_DISPLAYCHANGE is Windows only`);
    return;
  }
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    const before = await frameDpi(client);
    // a scale the window isn't at: 150% unless it already is
    const pct = before === 144 ? 125 : 150;
    const want = Math.ceil((96 * pct) / 100 / 4) * 4;

    await dpiRequest(client, `override ${pct}`);
    await sleep(200);
    if ((await frameDpi(client)) !== before) {
      throw new Error(`${name}: the DPI changed before any display message`);
    }
    sendMessage(frame, WM_DISPLAYCHANGE, 32, 0);
    await waitFrameDpi(client, want, "WM_DISPLAYCHANGE didn't pick up the new DPI");

    await dpiRequest(client, "override 0");
    sendMessage(frame, WM_DISPLAYCHANGE, 32, 0);
    await waitFrameDpi(client, before, "WM_DISPLAYCHANGE didn't go back to the real DPI");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
