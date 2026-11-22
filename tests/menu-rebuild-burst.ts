// ng rebuilt the whole menu model after every command, so a held key
// (scrolling, zooming) rebuilt it on every repeat. A burst of commands must
// cost at most a couple of rebuilds, and the menu must still catch up after.
//
// ng only: orig's menus are win32's, updated when they open.
// Run: bun tests/menu-rebuild-burst.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kBurst = 30;
const kMaxBuildsInBurst = 2;

async function menuBuilds(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, ["menubuilds", 0]))[1] ?? "");
  const m = /\bbuilds=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`menu-rebuild-burst: bad menubuilds reply: ${s}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP menu-rebuild-burst: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("menu-rebuild-burst");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makePdf(20, 400, 0, 0), "latin1");

  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle(30000);
    // let the startup rebuilds settle
    await sleep(500);
    const before = await menuBuilds(client);
    for (let i = 0; i < kBurst; i++) {
      await client.request(ControlCommand.TestInvokeCommand, [i % 2 === 0 ? "CmdScrollDown" : "CmdZoomIn"]);
    }
    const afterBurst = await menuBuilds(client);
    const inBurst = afterBurst - before;
    console.log(`menu-rebuild-burst: ${inBurst} menu rebuilds for ${kBurst} commands`);
    if (inBurst > kMaxBuildsInBurst) {
      throw new Error(
        `menu-rebuild-burst: ${inBurst} menu rebuilds for ${kBurst} commands, want <= ${kMaxBuildsInBurst}`,
      );
    }
    // the menu still catches up with what the commands changed (zoom checks)
    await pollUntil(
      async () => menuBuilds(client),
      (n) => n > afterBurst,
      { error: () => "menu-rebuild-burst: the menu was never rebuilt after the burst" },
    );
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("menu-rebuild-burst: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
