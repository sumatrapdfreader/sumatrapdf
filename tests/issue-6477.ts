// #6477: Open from URL... downloads the file into a folder and opens it.
// - a taken name gets a numbered one ("a doc (2).pdf")
// - a URL without a file name gets the extension of what was downloaded
// - a failed download leaves nothing behind
// - orig: the dialog's Download does the same
//
// The URLs are served by a local server, so this needs no network.
//
// Run: bun tests/issue-6477.ts [--no-build] [-ng]

import { existsSync, mkdirSync, readdirSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, makeTextPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, pressEnter, sendCommand } from "./win-automation.ts";
import { enumWindows, findChildWindow, getWindowPid, getWindowText, isWindowVisible, sendText } from "./winapi.ts";

const DLG_TITLE = "Open from URL";
const HTTP_NOT_FOUND = 404;

async function currentPath(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const m = /^path=(.+?) page=/.exec(String(res[1] ?? "").trim());
  return m ? resolve(m[1]!) : "";
}

async function openFromUrl(client: ControlClient, url: string, dir: string): Promise<void> {
  const res = await client.request(ControlCommand.TestOpenFromUrl, [url, dir]);
  if (res[0] !== 0) {
    throw new Error(`issue-6477: TestOpenFromUrl ${url} failed: ${String(res[1] ?? "")}`);
  }
}

async function waitForOpened(client: ControlClient, path: string): Promise<void> {
  await pollUntil(
    () => currentPath(client),
    (p) => p.toLowerCase() === resolve(path).toLowerCase(),
    { error: (p) => `issue-6477: expected ${path} to open, current tab: ${p}` },
  );
}

function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && getWindowText(hwnd) === DLG_TITLE) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

export async function testit(): Promise<void> {
  const pdf = makeTextPdf("downloaded");
  const server = Bun.serve({
    port: 0,
    fetch(req) {
      const path = new URL(req.url).pathname;
      if (path === "/files/a%20doc.pdf" || path === "/download") {
        return new Response(pdf, { headers: { "Content-Type": "application/pdf" } });
      }
      return new Response("not found", { status: HTTP_NOT_FOUND });
    },
  });
  const base = `http://127.0.0.1:${server.port}`;
  const dir = tmpPath("issue-6477-downloads");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const { proc, client, frame } = await launchControlled([]);
  try {
    await openFromUrl(client, `${base}/files/a%20doc.pdf`, dir);
    await waitForOpened(client, join(dir, "a doc.pdf"));

    await openFromUrl(client, `${base}/files/a%20doc.pdf?again=1`, dir);
    await waitForOpened(client, join(dir, "a doc (2).pdf"));

    await openFromUrl(client, `${base}/download?id=3`, dir);
    await waitForOpened(client, join(dir, "download.pdf"));

    await openFromUrl(client, `${base}/missing.pdf`, dir);
    // the failure is reported after the download thread is done
    await Bun.sleep(1000);
    if (existsSync(join(dir, "missing.pdf"))) {
      throw new Error(`issue-6477: a failed download left a file: ${readdirSync(dir).join(", ")}`);
    }

    // ng's dialog is a GPUI window
    if (USE_NG) {
      return;
    }
    // the folder field starts as the one used last
    sendCommand(frame, cmdId("CmdOpenFromURL"));
    const dlg = await pollUntil(
      () => findDialog(proc.pid!),
      (h) => h !== 0,
      { error: "issue-6477: no Open from URL dialog" },
    );
    const editUrl = findChildWindow(dlg, "Edit");
    sendText(editUrl, `${base}/files/a%20doc.pdf`);
    await pressEnter(editUrl);
    await waitForOpened(client, join(dir, "a doc (3).pdf"));
  } finally {
    client.close();
    await killAndWait(proc);
    server.stop(true);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
