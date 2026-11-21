// ng's home page marks documents that are on SumatraPDF Web with a Server
// badge ("Server ≠" when the sizes differ) and offers Upload to server /
// Delete from server, and Delete local for Delete File. A file only on the
// server is listed after the local ones and opening it downloads it. A local
// server stands in for the website.
//
// Run: bun tests/sumatrapdf-web-home.ts -ng [--no-build]   (or via tests/run-almost-all.ts)

import { copyFileSync, existsSync, mkdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, ROOT, USE_NG, cmdId, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { pressEnter, pressKey, sendCommand, waitForFrame } from "./win-automation.ts";
import { VK_DOWN, getWindowText } from "./winapi.ts";

const kPort = 9332;
const kKey = "so_abcdefghij_0123456789abcdef0123456789abcdef";
const kOnServer = "web-home-on-server.pdf";
const kLocalOnly = "web-home-local-only.pdf";
const kServerOnly = "web-home-server-only.pdf";

// the home list probe's server=: none, same size, different size
enum ServerState {
  None = 0,
  Same = 1,
  Differs = 2,
}

type Row = { path: string; server: ServerState };

async function homeRows(client: ControlClient): Promise<Row[] | null> {
  const res = await client.request(ControlCommand.TestHomeListRows, []);
  const out = String(res[1] ?? "");
  if (!out.startsWith("OK")) {
    return null;
  }
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

function serverState(rows: Row[] | null, name: string): ServerState | undefined {
  return rows?.find((r) => r.path.endsWith(name))?.server;
}

// the first entry's context menu, as "cmd\ttitle" rows
async function homeMenu(client: ControlClient): Promise<string[]> {
  const res = await client.request(ControlCommand.TestContextMenuAt, [0, 0]);
  return String(res[1] ?? "")
    .split("\n")
    .filter((l) => l)
    .map((l) => {
      const f = l.split("\t");
      return `${f[2]}\t${f[5]}`;
    });
}

function requireRow(menu: string[], row: string, present: boolean) {
  if (menu.includes(row) !== present) {
    throw new Error(`sumatrapdf-web-home: menu ${present ? "lacks" : "has"} '${row}':\n${menu.join("\n")}`);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("sumatrapdf-web-home: ng only, skipped");
    return;
  }
  const dir = tmpPath("sumatrapdf-web-home-docs");
  mkdirSync(dir, { recursive: true });
  const src = join(ROOT, "tests", "issue-5871.pdf");
  const onServer = join(dir, kOnServer);
  const localOnly = join(dir, kLocalOnly);
  copyFileSync(src, onServer);
  copyFileSync(src, localOnly);

  // name -> size; the server copy starts out with a different size
  const srcBytes = readFileSync(src);
  const files = new Map<string, number>([
    [kOnServer, statSync(onServer).size - 1],
    [kServerOnly, srcBytes.length],
  ]);
  const server = Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const name = url.searchParams.get("name") ?? "";
      if (url.pathname === "/api/files" && req.method === "GET") {
        return Response.json({ files: [...files].map(([n, size]) => ({ name: n, size })) });
      }
      if (url.pathname === "/api/files" && req.method === "POST") {
        const body = await req.arrayBuffer();
        if (body.byteLength === 0) {
          return new Response("unknown content", { status: 404 });
        }
        files.set(name, body.byteLength);
        return Response.json({ ok: true });
      }
      if (url.pathname === "/api/file" && req.method === "GET") {
        return files.has(name) ? new Response(srcBytes) : new Response("not found", { status: 404 });
      }
      if (url.pathname === "/api/file" && req.method === "DELETE") {
        if (!files.delete(name)) {
          return new Response("not found", { status: 404 });
        }
        return Response.json({ ok: true });
      }
      return new Response("not found", { status: 404 });
    },
  });

  try {
    const state = (p: string, n: number) => `\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${n}\n\t]`;
    const appdata = writeAppdata(
      "sumatrapdf-web-home",
      [
        "UiLanguage = en",
        "RestoreSession = false",
        "CheckForUpdates = false",
        "RememberOpenedFiles = true",
        "HomePageViewMode = list",
        `OnlineServer = http://127.0.0.1:${kPort}`,
        `OnlineApiKey = ${kKey}`,
        `FileStates [\n${state(onServer, 10)}\n${state(localOnly, 5)}\n]`,
      ].join("\n"),
    );

    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const frame = await waitForFrame(proc.pid!);
        await client.setNotificationsEnabled(false);

        const waitState = (want: ServerState, what: string) =>
          pollUntil(
            () => homeRows(client),
            (r) =>
              serverState(r, kOnServer) === want &&
              serverState(r, kLocalOnly) === ServerState.None &&
              serverState(r, kServerOnly) === ServerState.Same,
            { error: (r) => `sumatrapdf-web-home: ${what} ${JSON.stringify(r)}` },
          );
        const menuIs = async (upload: boolean, del: boolean) => {
          const menu = await homeMenu(client);
          requireRow(menu, "CmdDeleteFile\tDelete local", true);
          requireRow(menu, "CmdUploadToServer\tUpload to server", upload);
          requireRow(menu, "CmdDeleteFromServer\tDelete from server", del);
        };

        // a different size: Server ≠, the server copy can be replaced or deleted
        await waitState(ServerState.Differs, "no Server ≠ badge");
        await menuIs(true, true);

        // Upload to server replaces it
        sendCommand(frame, cmdId("CmdUploadToServer"));
        await waitState(ServerState.Same, "no Server badge after upload");
        if (files.get(kOnServer) !== statSync(onServer).size) {
          throw new Error("sumatrapdf-web-home: server did not get the upload");
        }
        await menuIs(false, true);

        // Delete from server drops the badge
        sendCommand(frame, cmdId("CmdDeleteFromServer"));
        await waitState(ServerState.None, "badge stayed after delete");
        if (files.has(kOnServer)) {
          throw new Error("sumatrapdf-web-home: server still has the file");
        }
        await menuIs(true, false);

        // the server-only file is the third row: Open and Delete from server only
        await pressKey(frame, VK_DOWN);
        await pressKey(frame, VK_DOWN);
        const menu = await homeMenu(client);
        requireRow(menu, "CmdOpenSelectedDocument\tOpen Document", true);
        requireRow(menu, "CmdDeleteFromServer\tDelete from server", true);
        requireRow(menu, "CmdDeleteFile\tDelete local", false);
        requireRow(menu, "CmdForgetSelectedDocument\tRemove From History", false);

        // opening it downloads it next to the app's data and opens it
        await pressEnter(frame);
        const downloaded = join(appdata, "SumatraPDF-web", kServerOnly);
        await pollUntil(
          () => existsSync(downloaded) && getWindowText(frame).includes(kServerOnly),
          (ok) => ok,
          { error: () => `sumatrapdf-web-home: not opened: '${getWindowText(frame)}' ${existsSync(downloaded)}` },
        );
        if (statSync(downloaded).size !== srcBytes.length) {
          throw new Error("sumatrapdf-web-home: downloaded copy has the wrong size");
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
