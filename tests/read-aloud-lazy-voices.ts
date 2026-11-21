// ng lists the TTS voices when a Read Aloud menu opens, not at startup: listing
// them starts the speech engine (SAPI / WinRT speech, audio), which costs
// memory and threads for a feature most sessions don't use.
//
// Run: bun tests/read-aloud-lazy-voices.ts -ng [--no-build]

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC, IS_WIN } from "./host.ts";
import { pollUntil, ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { listProcessModules } from "./winapi.ts";

const PDF = join(ROOT, "tests", "issue-2423.pdf");
const VK_MENU = 0x12;
const VK_RIGHT = 0x27;
const VK_DOWN = 0x28;
const kModAlt = 4;
const SPEECH_DLLS = ["sapi.dll", "sapi_onecore.dll", "windows.media.speech.dll"];

async function input(client: ControlClient, args: (string | number)[]): Promise<void> {
  const res = String((await client.request(ControlCommand.TestInput, args))[1] ?? "");
  if (!res.startsWith("OK")) {
    throw new Error(`read-aloud-lazy-voices: input ${JSON.stringify(args)}: ${res}`);
  }
}

function speechDllsLoaded(pid: number): string[] {
  const mods = listProcessModules(pid);
  return SPEECH_DLLS.filter((d) => mods.includes(d));
}

// rows: path \t kind \t cmd \t disabled \t checked \t title \t accel
async function menuRows(client: ControlClient): Promise<string[][]> {
  const res = String((await client.request(ControlCommand.TestMainMenu, []))[1] ?? "");
  return res
    .split("\n")
    .filter((l) => l.length > 0)
    .map((l) => l.split("\t"));
}

export async function testit(): Promise<void> {
  if (!USE_NG || !IS_WIN || IS_MAC) {
    console.log("SKIP read-aloud-lazy-voices: ng on Windows only (run with -ng)");
    return;
  }
  const { proc, client } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    const pid = proc.pid!;
    const early = speechDllsLoaded(pid);
    if (early.length > 0) {
      throw new Error(`read-aloud-lazy-voices: speech engine loaded at startup: ${early.join(", ")}`);
    }

    // the menu bar's titles, in order; the keyboard walks them by index
    const rows = await menuRows(client);
    const titles = rows.filter((r) => r[0] && !r[0].includes(".") && r[1] === "S");
    const idx = titles.findIndex((r) => r[5] === "Read Aloud");
    if (idx < 0) {
      throw new Error(`read-aloud-lazy-voices: no Read Aloud menu in ${titles.map((r) => r[5]).join(", ")}`);
    }

    // Alt arms the bar on its first title, Right moves along, Down opens
    await input(client, ["key", VK_MENU, kModAlt]);
    await input(client, ["keyup", VK_MENU, 0]);
    for (let i = 0; i < idx; i++) {
      await input(client, ["key", VK_RIGHT, 0]);
    }
    await input(client, ["key", VK_DOWN, 0]);

    const loaded = await pollUntil(
      async () => speechDllsLoaded(pid),
      (l) => l.length > 0,
      { error: () => "read-aloud-lazy-voices: opening the Read Aloud menu didn't list the voices" },
    );
    console.log(`read-aloud-lazy-voices: speech engine loaded on open: ${loaded.join(", ")}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("read-aloud-lazy-voices: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
