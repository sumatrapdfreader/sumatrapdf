// Opening a document from a slow (sleeping) drive must not freeze the UI: the
// engine is created on a thread while a "Loading" tab is shown, then the
// document goes on screen. -dbg-control TestLoadDelay simulates the drive.
// Covered opens: the way a reuse-instance launch sends it (kCopyDataOpen) and
// next file in folder, which also skips a file that fails to open, and the
// copy of a network-drive comic, whose progress the loading tab shows.
//
// Run: bun tests/open-slow-drive.ts [--no-build] [-ng]

import { createHash } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { extname, join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, runStandalone, tmpPath, writeStoredZip } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendOpenCopyData, sleep } from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const LOAD_DELAY_MS = 3000;
// 1x1 transparent PNG
const PNG = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

async function currentTab(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab);
  return String(res[1] ?? "");
}

// a Ping round-trip: how long the UI thread takes to answer
async function pingMs(client: ControlClient): Promise<number> {
  const t0 = Date.now();
  await client.request(ControlCommand.Ping);
  return Date.now() - t0;
}

// while the drive spins up the UI answers and shows the loading tab of name;
// then the drive wakes up and the document with name wantName loads
async function checkLoad(client: ControlClient, what: string, t0: number, name: string, wantName: string) {
  let sawLoading = false;
  // after a second the message says how long it has been loading: "... 1s"
  let sawElapsed = false;
  let worstPing = 0;
  while (Date.now() - t0 < LOAD_DELAY_MS / 2) {
    worstPing = Math.max(worstPing, await pingMs(client));
    const tab = await currentTab(client);
    if (tab.includes(name) && tab.includes("page=0")) {
      sawLoading = true;
    }
    if (/ msg=.* \d+s$/m.test(tab)) {
      sawElapsed = true;
    }
    await sleep(50);
  }
  if (!sawElapsed) {
    throw new Error(`open-slow-drive: ${what}: the loading message has no elapsed time (${await currentTab(client)})`);
  }
  if (worstPing >= LOAD_DELAY_MS / 2) {
    throw new Error(`open-slow-drive: ${what}: the UI didn't answer for ${worstPing} ms while loading`);
  }
  if (!sawLoading) {
    throw new Error(`open-slow-drive: ${what}: no loading tab (current tab: ${await currentTab(client)})`);
  }

  await client.request(ControlCommand.TestLoadDelay, [0]);
  const deadline = Date.now() + LOAD_DELAY_MS + 10000;
  let tab = "";
  while (Date.now() < deadline) {
    tab = await currentTab(client);
    if (tab.includes(wantName) && tab.includes("page=1")) {
      console.log(`open-slow-drive: ${what}: OK, loaded after ${Date.now() - t0} ms, worst ping ${worstPing} ms`);
      return;
    }
    await sleep(100);
  }
  throw new Error(`open-slow-drive: ${what}: the document didn't finish loading (current tab: ${tab})`);
}

async function testCopyDataOpen() {
  if (IS_MAC) {
    console.log("SKIP open-slow-drive copydata: sends a Windows WM_COPYDATA open");
    return;
  }
  const { proc, client, frame } = await launchControlled([]);
  try {
    await client.request(ControlCommand.Ping);
    await client.request(ControlCommand.TestLoadDelay, [LOAD_DELAY_MS]);
    const t0 = Date.now();
    if (!sendOpenCopyData(frame, PDF)) {
      throw new Error("open-slow-drive: the open request wasn't handled");
    }
    await checkLoad(client, "copydata", t0, "zlib.3.pdf", "zlib.3.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// bbb.pdf doesn't open: next file goes on to ccc.pdf
async function testNextFile() {
  const dir = tmpPath("open-slow-drive");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  copyFileSync(PDF, join(dir, "aaa.pdf"));
  writeFileSync(join(dir, "bbb.pdf"), "not a pdf");
  copyFileSync(PDF, join(dir, "ccc.pdf"));

  const { proc, client } = await launchControlled([join(dir, "aaa.pdf")]);
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestLoadDelay, [LOAD_DELAY_MS]);
    const t0 = Date.now();
    await client.request(ControlCommand.TestInvokeCommand, ["CmdOpenNextFileInFolder"]);
    await checkLoad(client, "next file", t0, "bbb.pdf", "ccc.pdf");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

// a comic on a network drive is copied to the local cache first; the loading
// tab shows the copy's progress. \\localhost\C$ is a network path.
async function testNetworkCopy() {
  if (IS_MAC) {
    console.log("SKIP open-slow-drive network copy: needs a Windows UNC path");
    return;
  }
  const dir = tmpPath("open-slow-drive-copy");
  const appData = tmpPath("open-slow-drive-appdata");
  rmSync(dir, { recursive: true, force: true });
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  // over the 32 MB a network comic is read into memory instead
  const filler = Buffer.alloc(40 * 1024 * 1024);
  writeStoredZip(join(dir, "big.cbz"), [
    { name: "001.png", data: PNG },
    { name: "filler.bin", data: filler },
  ]);
  const unc = join(dir, "big.cbz").replace(/^([A-Za-z]):/, "\\\\localhost\\$1$");
  if (!existsSync(unc)) {
    console.log(`SKIP open-slow-drive network copy: ${unc} isn't reachable (admin share disabled?)`);
    rmSync(dir, { recursive: true, force: true });
    return;
  }
  // orig caches outside -appdata, under a name from path and size: without
  // this every run after the first is a cache hit that copies nothing
  const cached = origCbxCachePath(unc, statSync(unc).size);
  rmSync(cached, { force: true });

  const { proc, client, frame } = await launchControlled(["-appdata", appData]);
  try {
    await client.request(ControlCommand.Ping);
    await client.request(ControlCommand.TestLoadDelay, [LOAD_DELAY_MS / 3]);
    const t0 = Date.now();
    if (!sendOpenCopyData(frame, unc)) {
      throw new Error("open-slow-drive: the network open request wasn't handled");
    }
    // TestLoadDelay holds the copy after its first progress report
    let worstPing = 0;
    let tab = "";
    const deadline = Date.now() + LOAD_DELAY_MS + 10000;
    while (Date.now() < deadline) {
      worstPing = Math.max(worstPing, await pingMs(client));
      tab = await currentTab(client);
      if (tab.includes("big.cbz") && tab.includes("copied=")) {
        break;
      }
      await sleep(50);
    }
    if (!tab.includes("copied=")) {
      throw new Error(`open-slow-drive: network copy: no copy progress (current tab: ${tab})`);
    }
    if (worstPing >= LOAD_DELAY_MS / 2) {
      throw new Error(`open-slow-drive: network copy: the UI didn't answer for ${worstPing} ms while copying`);
    }

    await client.request(ControlCommand.TestLoadDelay, [0]);
    while (Date.now() < deadline) {
      tab = await currentTab(client);
      if (tab.includes("big.cbz") && tab.includes("page=1")) {
        if (tab.includes("copied=")) {
          throw new Error(`open-slow-drive: network copy: progress outlived the load (${tab})`);
        }
        console.log(
          `open-slow-drive: network copy: OK, loaded after ${Date.now() - t0} ms, worst ping ${worstPing} ms`,
        );
        return;
      }
      await sleep(100);
    }
    throw new Error(`open-slow-drive: network copy: the comic didn't finish loading (current tab: ${tab})`);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
    rmSync(appData, { recursive: true, force: true });
    rmSync(cached, { force: true });
  }
}

// orig's GetCbxCachePathTemp: %LOCALAPPDATA%\SumatraPDF-data\cbx-cache\<md5("path|size")>.cbz
function origCbxCachePath(path: string, size: number): string {
  const name = createHash("md5").update(`${path}|${size}`).digest("hex") + extname(path);
  return join(process.env.LOCALAPPDATA ?? "", "SumatraPDF-data", "cbx-cache", name);
}

export async function testit(): Promise<void> {
  await testCopyDataOpen();
  await testNextFile();
  await testNetworkCopy();
}

if (import.meta.main) {
  await runStandalone(testit);
}
