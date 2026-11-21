// CmdSumatraPDFWeb opens the "Link SumatraPDF Web" dialog. With a saved API key
// it lists the account's files from the configured server; a local server
// stands in for the website, so the test needs no network.
//
// Run: bun tests/sumatrapdf-web-dialog.ts [--no-build] [-ng]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, USE_NG, cmdId, runStandalone, SLOW_BUILD_FACTOR, writeAppdata } from "./util.ts";
import { closeToolWindow, ngToolWindow, sendCommand, waitForFrame, waitToolWindow } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kPort = 9332;
const kListRowDy = 24;
const kTitle = "Link SumatraPDF Web";
const kToolName = "sumatrapdfweb";
const kKey = "so_abcdefghij_0123456789abcdef0123456789abcdef";
const kFiles = [
  { name: "alpha-web-test.pdf", size: 1234 },
  { name: "beta-web-test.pdf", size: 2048 },
];

async function waitFor(
  what: string,
  fn: () => boolean | Promise<boolean>,
  timeoutMs = 8000,
  detail: () => string = () => "",
): Promise<void> {
  const deadline = Date.now() + timeoutMs * SLOW_BUILD_FACTOR;
  while (!(await fn())) {
    if (Date.now() > deadline) {
      throw new Error(`sumatrapdf-web-dialog: ${what} ${detail()}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const auths: string[] = [];
  const server = Bun.serve({
    port: kPort,
    hostname: "127.0.0.1",
    fetch(req) {
      const url = new URL(req.url);
      if (url.pathname !== "/api/files") {
        return new Response("not found", { status: 404 });
      }
      auths.push(req.headers.get("authorization") ?? "");
      return Response.json({ files: kFiles });
    },
  });

  try {
    const appdata = writeAppdata(
      "sumatrapdf-web-dialog",
      [
        "UiLanguage = en",
        "RestoreSession = false",
        "ShowStartPage = false",
        "CheckForUpdates = false",
        `OnlineServer = http://127.0.0.1:${kPort}`,
        `OnlineApiKey = ${kKey}`,
      ].join("\n"),
    );
    const pdf = join(ROOT, "tests", "issue-5871.pdf");

    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const pid = proc.pid!;
        const frame = await waitForFrame(pid);
        await client.waitForRenderIdle(30000);
        await client.setNotificationsEnabled(false);

        sendCommand(frame, cmdId("CmdSumatraPDFWeb"));
        const dlg = await waitToolWindow(client, pid, kToolName, { title: kTitle });
        if (!dlg) {
          throw new Error(`sumatrapdf-web-dialog: no "${kTitle}" window after CmdSumatraPDFWeb`);
        }

        // opening with a saved key lists the files with that key
        await waitFor("the file list was not requested", () => auths.length > 0);
        if (auths[0] !== `Bearer ${kKey}`) {
          throw new Error(`sumatrapdf-web-dialog: Authorization is "${auths[0]}"`);
        }

        // a second command focuses the window, it does not open another
        sendCommand(frame, cmdId("CmdSumatraPDFWeb"));
        await sleep(300);
        if (USE_NG) {
          let raw = "";
          await waitFor(
            "the list does not show the files",
            async () => {
              raw = String((await client.request(ControlCommand.TestToolWindow, ["layout", kToolName]))[1] ?? "");
              // the rows are drawn text, not nodes: the list scrolls over one row per file
              const m = /scroll rect=\S+ content=\d+,(\d+)/.exec(raw);
              return Number(m?.[1] ?? 0) === kFiles.length * kListRowDy;
            },
            8000,
            () => raw,
          );
          const info = await ngToolWindow(client, kToolName);
          if (!info || info.title !== kTitle) {
            throw new Error("sumatrapdf-web-dialog: the window is gone or retitled");
          }
        }

        await closeToolWindow(client, kToolName, dlg);
        await waitFor(
          "the window did not close",
          async () => !(await waitToolWindow(client, pid, kToolName, { timeoutMs: 0 })),
        );
      },
      ["-appdata", appdata, "-window-pos", "1100x900@40x40", pdf],
    );
  } finally {
    server.stop(true);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
