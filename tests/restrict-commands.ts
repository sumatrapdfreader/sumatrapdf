// Commands that also work without a document ("Check for Updates", "Options")
// must still be hidden when sumatrapdfrestrict.ini denies their permission.
//
// Copies the exe next to a restrict ini, opens a PDF and counts the rows the
// command palette offers for each command.
//
// Run: bun tests/restrict-commands.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { withControlledSumatra, type ControlClient } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import {
  paletteSetText,
  paletteState,
  pressEscape,
  sendCommand,
  waitForFrame,
  waitForPalette,
} from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF = resolve("ext/a-zlib/zlib.3.pdf");
const CHECK_UPDATES = ">check for updates";
const OPTIONS = ">options";

const SIDE_BY_SIDE_DLLS = ["libsumatrapdf.dll", "clang_rt.asan_dynamic-x86_64.dll"].map((name) =>
  join(dirname(EXE), name),
);

function iniWith(overrides: Record<string, number>): string {
  const policies: Record<string, number> = {
    InternetAccess: 1,
    DiskAccess: 1,
    SavePreferences: 1,
    RegistryAccess: 1,
    PrinterAccess: 1,
    CopySelection: 1,
    FullscreenAccess: 1,
    ...overrides,
  };
  const lines = ["[Policies]"];
  for (const [k, v] of Object.entries(policies)) {
    lines.push(`${k} = ${v}`);
  }
  return lines.join("\n") + "\n";
}

async function paletteItems(client: ControlClient, frame: number, query: string): Promise<number> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const deadline = Date.now() + 8000;
  let typed = false;
  for (;;) {
    const state = await paletteState(client);
    if (state.open) {
      if (!typed) {
        await paletteSetText(client, await waitForPalette(client, frame), query);
        typed = true;
      }
      if (state.queryLen === query.length) {
        const palette = await waitForPalette(client, frame);
        pressEscape(palette.edit);
        for (let i = 0; i < 100 && (await paletteState(client)).open; i++) {
          await sleep(50);
        }
        return state.items;
      }
    }
    if (Date.now() > deadline) {
      throw new Error(`restrict-commands: palette never showed '${query}': ${state.raw}`);
    }
    await sleep(50);
  }
}

// want: [check for updates rows, options rows]
async function runCase(name: string, ini: string, want: [boolean, boolean]): Promise<void> {
  const appDir = tmpPath(`restrict-commands-${name}`);
  rmSync(appDir, { recursive: true, force: true });
  mkdirSync(appDir, { recursive: true });
  const testExe = join(appDir, "SumatraPDF.exe");
  copyFileSync(EXE, testExe);
  for (const dll of SIDE_BY_SIDE_DLLS) {
    if (existsSync(dll)) {
      copyFileSync(dll, join(appDir, basename(dll)));
    }
  }
  writeFileSync(join(appDir, "sumatrapdfrestrict.ini"), ini);

  await withControlledSumatra(
    testExe,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      const updates = await paletteItems(client, frame, CHECK_UPDATES);
      const options = await paletteItems(client, frame, OPTIONS);
      console.log(`restrict-commands: ${name}: ${updates} update rows, ${options} options rows`);
      if (updates > 0 !== want[0]) {
        throw new Error(`restrict-commands: ${name}: ${updates} 'check for updates' rows, want shown=${want[0]}`);
      }
      if (options > 0 !== want[1]) {
        throw new Error(`restrict-commands: ${name}: ${options} 'options' rows, want shown=${want[1]}`);
      }
    },
    [PDF],
    { cwd: appDir },
  );
}

export async function testit(): Promise<void> {
  await runCase("allowed", iniWith({}), [true, true]);
  await runCase("no-internet", iniWith({ InternetAccess: 0 }), [false, true]);
  await runCase("no-prefs", iniWith({ SavePreferences: 0 }), [true, false]);
}

if (import.meta.main) {
  await runStandalone(testit);
}
