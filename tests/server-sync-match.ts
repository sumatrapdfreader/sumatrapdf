// Local and SumatraPDF Web files match by content once hashed
// (AGENTS.md, "SumatraPDF Web"):
//   - a renamed copy of a server file gets the Server badge, and that server
//     file isn't listed again as server-only
//   - a file with a server file's name and size but other content is Server ≠
//     (name and size alone would say Server)
//   - Delete from server on the renamed copy deletes the server's name
// A local server stands in for the website.
//
// Run: bun tests/server-sync-match.ts [-ng] [--no-build]

import { createHash } from "node:crypto";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import {
  cmdId,
  EXE,
  ROOT,
  USE_NG,
  pollUntil,
  runStandalone,
  SLOW_BUILD_FACTOR,
  tmpPath,
  writeAppdata,
} from "./util.ts";
import { pressEnter, sendCommand, waitForFrame } from "./win-automation.ts";
import { WM_COMMAND, postMessage, waitForTopWindow } from "./winapi.ts";

const name = "server-sync-match";
const kPort = 9332;
const kKey = "so_abcdefghij_0123456789abcdef0123456789abcdef";
const IDYES = 6;

// the home list probe's server=: none, same, differs
enum ServerState {
  None = 0,
  Same = 1,
  Differs = 2,
}

type Row = { path: string; server: ServerState };

const sha1 = (b: Uint8Array) => createHash("sha1").update(b).digest("hex");

async function homeRows(client: ControlClient): Promise<Row[]> {
  const out = String((await client.request(ControlCommand.TestHomeListRows, []))[1] ?? "");
  const rows: Row[] = [];
  for (const line of out.split("\n")) {
    const row = /^row=\d+ .* path=(.*)$/.exec(line);
    if (row) {
      rows.push({ path: row[1]!, server: ServerState.None });
      continue;
    }
    const srv = /^server=(\d)$/.exec(line);
    if (srv && rows.length > 0) {
      rows[rows.length - 1]!.server = Number(srv[1]) as ServerState;
    }
  }
  return rows;
}

const rowsNamed = (rows: Row[], file: string) => rows.filter((r) => r.path.endsWith(file));

// Yes to the question: ng's in-window box takes Enter, orig's is a Win32 box
async function answerYes(frame: number, pid: number) {
  if (USE_NG) {
    await pressEnter(frame);
    return;
  }
  const dlg = await waitForTopWindow(pid, "#32770", 5000);
  if (!dlg) {
    throw new Error(`${name}: no question asked`);
  }
  postMessage(dlg, WM_COMMAND, IDYES, 0);
}

export async function testit(): Promise<void> {
  const dir = tmpPath(`${name}-docs`);
  mkdirSync(dir, { recursive: true });
  const bytes = readFileSync(join(ROOT, "tests", "issue-5871.pdf"));
  // same size, other content: the server's same-name.pdf, and the local one
  const other = Uint8Array.from(bytes);
  other[100] = other[100]! ^ 0xff;
  const localOther = Uint8Array.from(bytes);
  localOther[200] = localOther[200]! ^ 0xff;

  const copy = join(dir, "renamed-copy.pdf");
  const sameName = join(dir, "same-name.pdf");
  writeFileSync(copy, bytes);
  writeFileSync(sameName, localOther);

  const files = new Map<string, Uint8Array>([
    ["notes.pdf", bytes],
    ["same-name.pdf", other],
  ]);
  const deleted: string[] = [];
  const server = Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    fetch(req) {
      const url = new URL(req.url);
      const fileName = url.searchParams.get("name") ?? "";
      if (url.pathname === "/api/files" && req.method === "GET") {
        const list = [...files].map(([n, b]) => ({ name: n, size: b.length, sha1: sha1(b) }));
        return Response.json({ files: list });
      }
      if (url.pathname === "/api/file" && req.method === "DELETE") {
        deleted.push(fileName);
        files.delete(fileName);
        return Response.json({ ok: true });
      }
      return new Response("not found", { status: 404 });
    },
  });

  try {
    const state = (p: string) => `\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = 5\n\t]`;
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
        `FileStates [\n${state(copy)}\n${state(sameName)}\n]`,
      ].join("\n"),
    );

    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const frame = await waitForFrame(proc.pid!);
        await client.setNotificationsEnabled(false);

        // once hashed: the copy is notes.pdf, same-name.pdf differs, and
        // notes.pdf isn't a server-only row
        await pollUntil(
          () => homeRows(client),
          (rows) =>
            rowsNamed(rows, "renamed-copy.pdf")[0]?.server === ServerState.Same &&
            rowsNamed(rows, "same-name.pdf").length === 1 &&
            rowsNamed(rows, "same-name.pdf")[0]?.server === ServerState.Differs &&
            rowsNamed(rows, "notes.pdf").length === 0,
          { timeoutMs: 15_000 * SLOW_BUILD_FACTOR, error: (rows) => `${name}: rows:\n${JSON.stringify(rows)}` },
        );

        // the copy is the first row, selected: Delete from server deletes notes.pdf
        await client.request(ControlCommand.TestContextMenuAt, [0, 0]);
        sendCommand(frame, cmdId("CmdDeleteFromServer"));
        await answerYes(frame, proc.pid!);
        await pollUntil(
          () => deleted.join(","),
          (d) => d === "notes.pdf",
          { error: (d) => `${name}: deleted '${d}', want notes.pdf` },
        );
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
