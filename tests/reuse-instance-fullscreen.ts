// -presentation / -fullscreen must reach the running instance when the file is
// handed over to it (-reuse-instance) instead of being dropped.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const PDF = join(ROOT, "tests", "issue-1189.pdf");

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

type Mode = { presentation: boolean; fullscreen: boolean };

async function mode(client: ControlClient): Promise<Mode> {
  const out = String((await client.request(ControlCommand.TestDisplayMode, ["get"]))[1] ?? "");
  const m = /presentation=(\d) fullscreen=(\d)/.exec(out);
  if (!m) {
    throw new Error(`reuse-instance-fullscreen: can't parse: ${out}`);
  }
  return { presentation: m[1] === "1", fullscreen: m[2] === "1" };
}

async function waitForMode(client: ControlClient, what: string, pred: (m: Mode) => boolean): Promise<void> {
  const deadline = Date.now() + 8000;
  let last: Mode | null = null;
  while (Date.now() < deadline) {
    last = await mode(client);
    if (pred(last)) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`reuse-instance-fullscreen: ${what}; mode is ${JSON.stringify(last)}`);
}

async function openInRunning(appdata: string, flag: string): Promise<void> {
  const p = Bun.spawn([EXE, "-appdata", appdata, "-reuse-instance", flag, PDF], { stdout: "ignore", stderr: "ignore" });
  await p.exited;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("reuse-instance-fullscreen");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      await waitForMode(client, "starts windowed", (m) => !m.presentation && !m.fullscreen);

      await openInRunning(dir, "-presentation");
      await waitForMode(client, "-presentation was ignored", (m) => m.presentation && !m.fullscreen);

      await openInRunning(dir, "-fullscreen");
      await waitForMode(client, "-fullscreen was ignored", (m) => !m.presentation && m.fullscreen);
    },
    ["-appdata", dir, PDF],
    { defaultWindowPos: true },
  );
  console.log("reuse-instance-fullscreen: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
