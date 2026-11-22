// ng built every command palette row (hundreds: commands, files, settings) on
// every frame. Only the rows in view are built now, with spacers for the rest.
// Moving the selection down past the first screen must keep the selected row
// built and visible.
//
// ng only: it counts the built rows through -dbg-control.
// Run: bun tests/command-palette-rows-built.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, paletteState, sendCommand } from "./win-automation.ts";
import { sleep, VK_DOWN } from "./winapi.ts";

const kDowns = 40;

async function rowsBuilt(client: Parameters<typeof paletteState>[0]): Promise<number> {
  const s = (await paletteState(client)).raw;
  const m = /\browsBuilt=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`command-palette-rows-built: no rowsBuilt in: ${s}`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP command-palette-rows-built: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("command-palette-rows-built");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "a.pdf");
  writeFileSync(pdf, makePdf(2, 601, 0, 0), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));
    const st = await pollUntil(
      () => paletteState(client),
      (s) => s.open && s.items > 100,
      { error: (s) => `command-palette-rows-built: palette not open with many rows: ${s.raw}` },
    );
    await sleep(300);
    const before = await rowsBuilt(client);
    for (let i = 0; i < kDowns; i++) {
      await client.request(ControlCommand.TestInput, ["key", VK_DOWN, 0, 0, 0]);
    }
    const after = await pollUntil(
      () => paletteState(client),
      (s) => s.sel === kDowns,
      { error: (s) => `command-palette-rows-built: selection didn't move down: ${s.raw}` },
    );
    await sleep(300);
    const perKey = ((await rowsBuilt(client)) - before) / kDowns;
    console.log(
      `command-palette-rows-built: ${perKey.toFixed(1)} rows built per key for ${st.items} rows, sel=${after.sel}`,
    );
    // a frame per key at most, each building the rows in view
    if (perKey > st.items / 4) {
      throw new Error(`command-palette-rows-built: ${perKey} rows built per key for ${st.items} rows`);
    }
    if (!/selText=\S/.test(after.raw)) {
      throw new Error(`command-palette-rows-built: no selected row: ${after.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("command-palette-rows-built: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
