// ng's home page marks documents that are on SumatraPDF Web with a Server
// badge and offers Upload to server / Delete from server, and Delete local for
// Delete File. A local server stands in for the website.
//
// Run: bun tests/sumatrapdf-web-home.ts -ng [--no-build]   (or via tests/run-almost-all.ts)

import { copyFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, ROOT, USE_NG, cmdId, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";

const kPort = 9332;
const kKey = "so_abcdefghij_0123456789abcdef0123456789abcdef";
const kOnServer = "web-home-on-server.pdf";
const kLocalOnly = "web-home-local-only.pdf";

type Row = { path: string; server: boolean };

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
      rows.push({ path: row[1]!, server: false });
      continue;
    }
    const srv = /^server=(\d)$/.exec(line);
    if (srv && rows.length > 0) {
      rows[rows.length - 1]!.server = srv[1] === "1";
    }
  }
  return rows;
}

function serverFlag(rows: Row[] | null, name: string): boolean | undefined {
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
  const names = new Set<string>([kOnServer]);
  const server = Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const name = url.searchParams.get("name") ?? "";
      if (url.pathname === "/api/files" && req.method === "GET") {
        return Response.json({ files: [...names].map((n) => ({ name: n, size: 1 })) });
      }
      if (url.pathname === "/api/files" && req.method === "POST") {
        const body = await req.arrayBuffer();
        if (body.byteLength === 0) {
          return new Response("unknown content", { status: 404 });
        }
        names.add(name);
        return Response.json({ ok: true });
      }
      if (url.pathname === "/api/file" && req.method === "DELETE") {
        if (!names.delete(name)) {
          return new Response("not found", { status: 404 });
        }
        return Response.json({ ok: true });
      }
      return new Response("not found", { status: 404 });
    },
  });

  try {
    const dir = tmpPath("sumatrapdf-web-home-docs");
    mkdirSync(dir, { recursive: true });
    const src = join(ROOT, "tests", "issue-5871.pdf");
    const onServer = join(dir, kOnServer);
    const localOnly = join(dir, kLocalOnly);
    copyFileSync(src, onServer);
    copyFileSync(src, localOnly);
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

        let rows = await pollUntil(
          () => homeRows(client),
          (r) => serverFlag(r, kOnServer) === true && serverFlag(r, kLocalOnly) === false,
          { error: (r) => `sumatrapdf-web-home: badges ${JSON.stringify(r)}` },
        );

        // the first entry is on the server
        let menu = await homeMenu(client);
        requireRow(menu, "CmdDeleteFile\tDelete local", true);
        requireRow(menu, "CmdDeleteFromServer\tDelete from server", true);
        requireRow(menu, "CmdUploadToServer\tUpload to server", false);

        // Delete from server drops the badge and offers Upload to server
        sendCommand(frame, cmdId("CmdDeleteFromServer"));
        rows = await pollUntil(
          () => homeRows(client),
          (r) => serverFlag(r, kOnServer) === false,
          { error: (r) => `sumatrapdf-web-home: badge stayed after delete ${JSON.stringify(r)}` },
        );
        if (names.has(kOnServer)) {
          throw new Error("sumatrapdf-web-home: server still has the file");
        }
        menu = await homeMenu(client);
        requireRow(menu, "CmdUploadToServer\tUpload to server", true);
        requireRow(menu, "CmdDeleteFromServer\tDelete from server", false);

        // Upload to server brings it back
        sendCommand(frame, cmdId("CmdUploadToServer"));
        await pollUntil(
          () => homeRows(client),
          (r) => serverFlag(r, kOnServer) === true,
          { error: (r) => `sumatrapdf-web-home: no badge after upload ${JSON.stringify(r)}` },
        );
        if (!names.has(kOnServer)) {
          throw new Error("sumatrapdf-web-home: server did not get the upload");
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
