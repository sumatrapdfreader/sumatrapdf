// An auto-reload of a file that changed on disk loads the new version on a
// background thread: while it loads (slow drive simulated with TestLoadDelay)
// the UI answers and still shows the old version, then the new one is swapped
// in. A change during that load ends with the latest version shown.
//
// Run: bun tests/auto-reload-background.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const LOAD_DELAY_MS = 4000;

async function lastPage(client: ControlClient): Promise<number> {
  await client.request(ControlCommand.TestInvokeCommand, ["CmdGoToLastPage"]);
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const m = /page=(\d+)\s*$/.exec(String(res[1] ?? ""));
  return m ? Number(m[1]) : -1;
}

async function waitForLastPage(client: ControlClient, page: number, timeoutMs: number): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const got = await lastPage(client);
    if (got === page) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`auto-reload-background: expected ${page} pages, have ${got}`);
    }
    await sleep(100);
  }
}

function writePdfAtomically(dir: string, dst: string, pages: number): void {
  const tmp = join(dir, `tmp-${pages}.pdf`);
  writeFileSync(tmp, makePdf(pages, 400, 0, 0), "latin1");
  copyFileSync(tmp, dst);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("auto-reload-background");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(1, 400, 0, 0), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await waitForLastPage(client, 1, 5000);
    await client.request(ControlCommand.TestLoadDelay, [LOAD_DELAY_MS]);

    writePdfAtomically(dir, pdf, 2);
    // the reload starts after the watcher's delay and one stable timer tick
    await sleep(2000);

    // the reload is sleeping in TestLoadDelay: the UI must answer right away
    // and still show the old version
    const t = Date.now();
    const page = await lastPage(client);
    const ms = Date.now() - t;
    if (ms > 1000) {
      throw new Error(`auto-reload-background: UI blocked for ${ms} ms during the reload`);
    }
    if (page !== 1) {
      throw new Error(`auto-reload-background: expected the old 1-page version while loading, have ${page}`);
    }

    // changes again while the first reload is loading
    writePdfAtomically(dir, pdf, 3);
    await waitForLastPage(client, 3, 3 * LOAD_DELAY_MS + 5000);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("auto-reload-background: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
