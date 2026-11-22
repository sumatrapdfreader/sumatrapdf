// The last SumatraPDF Web list is kept on disk and listed again once stale
// (AGENTS.md, "SumatraPDF Web"):
//   1. a list is shown and cached; a list older than the re-list interval is
//      fetched again when the window is activated
//   2. with the server failing, the cached list still shows
//   3. another account's key ignores the cache
// A local server stands in for the website.
//
// Run: bun tests/server-sync-cache.ts [-ng] [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, ROOT, pollUntil, runStandalone, writeAppdata } from "./util.ts";
import { waitForFrame } from "./win-automation.ts";
import { postMessage } from "./winapi.ts";

const name = "server-sync-cache";
const kPort = 9332;
const kKey = "so_abcdefghij_0123456789abcdef0123456789abcdef";
const kOtherKey = "so_otheruser1_0123456789abcdef0123456789abcdef";
const WM_ACTIVATE = 0x0006;
const WA_ACTIVE = 1;
const WM_MOUSEMOVE = 0x0200;

async function homePaths(client: ControlClient): Promise<string[]> {
  const out = String((await client.request(ControlCommand.TestHomeListRows, []))[1] ?? "");
  const paths: string[] = [];
  for (const line of out.split("\n")) {
    const row = /^row=\d+ .* path=(.*)$/.exec(line);
    if (row) {
      paths.push(row[1]!);
    }
  }
  return paths;
}

const has = (paths: string[], file: string) => paths.some((p) => p.endsWith(file));

function settings(key: string): string {
  return [
    "UiLanguage = en",
    "RestoreSession = false",
    "CheckForUpdates = false",
    "RememberOpenedFiles = true",
    "HomePageViewMode = list",
    `OnlineServer = http://127.0.0.1:${kPort}`,
    `OnlineApiKey = ${key}`,
    // a history entry, so the home page lists files
    `FileStates [\n\t[\n\t\tFilePath = ${join(ROOT, "tests", "issue-5871.pdf")}\n\t\tOpenCount = 5\n\t]\n]`,
  ].join("\n");
}

export async function testit(): Promise<void> {
  const bytes = readFileSync(join(ROOT, "tests", "issue-5871.pdf"));
  let names = ["cached.pdf"];
  let failing = false;
  const server = Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    fetch(req) {
      const url = new URL(req.url);
      if (url.pathname === "/api/files" && req.method === "GET") {
        if (failing) {
          return new Response("unavailable", { status: 503 });
        }
        // other content than the history file, so they don't match
        return Response.json({ files: names.map((n) => ({ name: n, size: bytes.length - 1 })) });
      }
      return new Response("not found", { status: 404 });
    },
  });

  const appdata = writeAppdata(name, settings(kKey));
  const args = ["-appdata", appdata, "-window-pos", "1100x900@40x40"];
  try {
    // 1. listed and cached; listed again once stale
    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const frame = await waitForFrame(proc.pid!);
        await client.request(ControlCommand.TestContentHash, ["hooks", 0, 0, "", 500]);
        await pollUntil(
          () => homePaths(client),
          (p) => has(p, "cached.pdf"),
          { error: (p) => `${name}: no server-only row:\n${p.join("\n")}` },
        );
        names = ["cached.pdf", "added.pdf"];
        await pollUntil(
          () => {
            postMessage(frame, WM_ACTIVATE, WA_ACTIVE, 0);
            postMessage(frame, WM_MOUSEMOVE, 0, 0);
            return homePaths(client);
          },
          (p) => has(p, "added.pdf"),
          { intervalMs: 250, error: (p) => `${name}: not listed again:\n${p.join("\n")}` },
        );
      },
      args,
    );

    // 2. the server fails: the cached list shows
    failing = true;
    await withControlledSumatra(
      EXE,
      async (client) => {
        await pollUntil(
          () => homePaths(client),
          (p) => has(p, "cached.pdf") && has(p, "added.pdf"),
          { error: (p) => `${name}: cached list not shown:\n${p.join("\n")}` },
        );
      },
      args,
    );

    // 3. another account: not its cache
    writeFileSync(join(appdata, "SumatraPDF-settings.txt"), `${settings(kOtherKey)}\n`);
    await withControlledSumatra(
      EXE,
      async (client) => {
        // the history row shows; give a cache that would wrongly load time to
        await pollUntil(
          () => homePaths(client),
          (p) => p.length > 0,
        );
        await Bun.sleep(1000);
        const paths = await homePaths(client);
        if (has(paths, "cached.pdf")) {
          throw new Error(`${name}: another account's cache shown:\n${paths.join("\n")}`);
        }
      },
      args,
    );
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
