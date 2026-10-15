// Issue #6314: environments that forbid AI need a way to switch the AI
// features off. sumatrapdfrestrict.ini gets an AIChat policy; AI chat and AI
// translation are also off when InternetAccess or DiskAccess is 0, or when
// the ini exists without an AIChat = 1 line.
//
// Copies the exe next to a restrict ini, opens a PDF and counts the chat rows
// the command palette offers (orig "AI Claude chat with document", ng "Claude
// chat..."): the four backends when AI is allowed, none when it is not.
//
// Run: bun tests/issue-6314.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { paletteSetText, sendCommand, waitForFrame, waitForPalette, paletteState } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF = resolve("ext/a-zlib/zlib.3.pdf");
const AI_QUERY = ">chat";
const AI_BACKENDS = 4;

// what the exe needs beside it once copied out of the build directory
const SIDE_BY_SIDE_DLLS = ["libsumatrapdf.dll", "clang_rt.asan_dynamic-x86_64.dll"].map((name) =>
  join(dirname(EXE), name),
);

function allowAll(overrides: Record<string, number>): string {
  const policies: Record<string, number> = {
    InternetAccess: 1,
    DiskAccess: 1,
    SavePreferences: 1,
    RegistryAccess: 1,
    PrinterAccess: 1,
    CopySelection: 1,
    FullscreenAccess: 1,
    AIChat: 1,
    ...overrides,
  };
  const lines = ["[Policies]"];
  for (const [k, v] of Object.entries(policies)) {
    if (v >= 0) {
      lines.push(`${k} = ${v}`);
    }
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
        return state.items;
      }
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6314: palette never showed '${query}': ${state.raw}`);
    }
    await sleep(50);
  }
}

async function runCase(name: string, ini: string, wantAiRows: number, wantAiBit: number): Promise<void> {
  const appDir = tmpPath(`issue-6314-${name}`);
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
      const res = await client.request(ControlCommand.TestGetPolicies);
      const raw = String(res[1] ?? "");
      const bit = /^aichat=(\d)/m.exec(raw);
      if (res[0] !== 0 || !bit) {
        throw new Error(`issue-6314: ${name}: TestGetPolicies has no aichat bit: ${raw.trim()}`);
      }
      if (+bit[1]! !== wantAiBit) {
        throw new Error(`issue-6314: ${name}: aichat=${bit[1]}, want ${wantAiBit}\n${raw}`);
      }
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      const n = await paletteItems(client, frame, AI_QUERY);
      if (n !== wantAiRows) {
        throw new Error(`issue-6314: ${name}: the palette offers ${n} AI chat rows, want ${wantAiRows}`);
      }
      console.log(`issue-6314: ${name}: aichat=${bit[1]}, ${n} AI chat rows`);
    },
    [PDF],
    { cwd: appDir },
  );
}

export async function testit(): Promise<void> {
  await runCase("allowed", allowAll({}), AI_BACKENDS, 1);
  await runCase("aichat-off", allowAll({ AIChat: 0 }), 0, 0);
  await runCase("aichat-missing", allowAll({ AIChat: -1 }), 0, 0);
  await runCase("no-internet", allowAll({ InternetAccess: 0 }), 0, 1);
  await runCase("no-disk", allowAll({ DiskAccess: 0 }), 0, 1);
}

if (import.meta.main) {
  await runStandalone(testit);
}
