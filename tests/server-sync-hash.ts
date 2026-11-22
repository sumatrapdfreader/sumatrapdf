// History files are hashed in the background to match SumatraPDF Web files by
// content (AGENTS.md, "SumatraPDF Web"):
//   - a copy of a server file under another name gets hashed and stored
//   - a file whose size no server file has isn't read
//   - a stored hash whose size and mtime still match is reused, not recomputed
//   - a file on a share that stops answering is given up on (unreachable)
// A local server stands in for the website.
//
// Run: bun tests/server-sync-hash.ts [-ng] [--no-build]

import { createHash } from "node:crypto";
import { copyFileSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, ROOT, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath, writeAppdata } from "./util.ts";

const name = "server-sync-hash";
const kPort = 9332;
const kKey = "so_abcdefghij_0123456789abcdef0123456789abcdef";
const kFileTimeUnixEpoch = 116444736000000000n;

const sha1 = (b: Uint8Array) => createHash("sha1").update(b).digest("hex");

// FILETIME (100 ns since 1601) of the file's modification time, as stored
function fileTime(path: string): bigint {
  return statSync(path, { bigint: true }).mtimeNs / 100n + kFileTimeUnixEpoch;
}

function statusOf(dump: string, file: string): string {
  for (const line of dump.split("\n")) {
    const m = /^status=(\S+) path=(.*)$/.exec(line);
    if (m && m[2]!.endsWith(file)) {
      return m[1]!;
    }
  }
  return "";
}

async function hashStatus(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestContentHash, ["status"]))[1] ?? "");
}

async function storedHashes(client: ControlClient): Promise<Map<string, string>> {
  const out = String((await client.request(ControlCommand.TestHomeListRows, []))[1] ?? "");
  const res = new Map<string, string>();
  let path = "";
  for (const line of out.split("\n")) {
    const row = /^row=\d+ .* path=(.*)$/.exec(line);
    if (row) {
      path = row[1]!;
      continue;
    }
    const h = /^hash=(.*)$/.exec(line);
    if (h && path) {
      res.set(path, h[1]!);
    }
  }
  return res;
}

export async function testit(): Promise<void> {
  const dir = tmpPath(`${name}-docs`);
  mkdirSync(dir, { recursive: true });
  const src = join(ROOT, "tests", "issue-5871.pdf");
  const bytes = readFileSync(src);

  const copy = join(dir, "renamed-copy.pdf");
  const otherSize = join(dir, "other-size.pdf");
  const reused = join(dir, "stored-hash.pdf");
  const stalled = join(dir, "stall-share.pdf");
  copyFileSync(src, copy);
  writeFileSync(otherSize, bytes.subarray(0, 1000));
  copyFileSync(src, reused);
  copyFileSync(src, stalled);
  // a stored hash that isn't the content's: reusing it (not reading the file)
  // is the only way it can come back
  const fakeSha1 = "0123456789abcdef0123456789abcdef01234567";

  // the list waits for the test hooks, so hashing starts after them
  let releaseList: () => void = () => {};
  const listReady = new Promise<void>((resolve) => (releaseList = resolve));
  const server = Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      if (url.pathname === "/api/files" && req.method === "GET") {
        await listReady;
        return Response.json({ files: [{ name: "on-server.pdf", size: bytes.length, sha1: sha1(bytes) }] });
      }
      return new Response("not found", { status: 404 });
    },
  });

  try {
    const state = (p: string, extra = "") => `\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = 5\n${extra}\t]`;
    // newest first: the stalled file is hashed last, so its share being given
    // up on can't hide the others
    const states = [
      state(copy),
      state(otherSize),
      state(reused, `\t\tContentSha1 = ${fakeSha1} ${bytes.length} ${fileTime(reused)}\n`),
      state(stalled),
    ];
    const appdata = writeAppdata(
      name,
      [
        "UiLanguage = en",
        "RestoreSession = false",
        "CheckForUpdates = false",
        "RememberOpenedFiles = true",
        "HomePageViewMode = list",
        `OnlineServer = http://127.0.0.1:${kPort}`,
        `OnlineApiKey = ${kKey}`,
        `FileStates [\n${states.join("\n")}\n]`,
      ].join("\n"),
    );

    await withControlledSumatra(
      EXE,
      async (client) => {
        await client.request(ControlCommand.TestContentHash, ["hooks", 500, 10_000, "stall-share"]);
        releaseList();

        const want = new Map([
          ["renamed-copy.pdf", "hashed"],
          ["other-size.pdf", "size-not-on-server"],
          ["stored-hash.pdf", "hashed"],
          ["stall-share.pdf", "unreachable"],
        ]);
        await pollUntil(
          () => hashStatus(client),
          (dump) => [...want].every(([f, st]) => statusOf(dump, f) === st),
          { timeoutMs: 15_000 * SLOW_BUILD_FACTOR, error: (dump) => `${name}: hash states:\n${dump}` },
        );

        const hashes = await storedHashes(client);
        const hashOf = (p: string) => [...hashes].find(([k]) => k.endsWith(p))?.[1] ?? "";
        if (!hashOf("renamed-copy.pdf").startsWith(`${sha1(bytes)} ${bytes.length} `)) {
          throw new Error(`${name}: copy not hashed: '${hashOf("renamed-copy.pdf")}'`);
        }
        if (!hashOf("stored-hash.pdf").startsWith(fakeSha1)) {
          throw new Error(`${name}: stored hash not reused: '${hashOf("stored-hash.pdf")}'`);
        }
        if (hashOf("other-size.pdf") !== "" || hashOf("stall-share.pdf") !== "") {
          throw new Error(`${name}: hashed what it shouldn't have:\n${[...hashes].join("\n")}`);
        }
      },
      ["-appdata", appdata, "-window-pos", "1100x900@40x40"],
    );
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
