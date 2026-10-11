// PR #6316: ng replaced the document with an error page when an auto-reload
// hit a file caught mid-rewrite, and then never reloaded it again. Like orig,
// a failed auto-reload must keep the document and retry on the next change.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { Socket } from "node:net";
import { connect, controlCommands as C, sendCommand } from "../cmd/ng-dbg-control";
import { EXE, USE_NG, makePdf, runStandalone, tmpPath } from "./util";
import { windowPosArgs } from "./win-automation";
import { killAndWait, sleep } from "./winapi";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
ReloadModifiedDocuments = true
`;

const kWaitMs = 15000;

async function currentTab(socket: Socket): Promise<string> {
  const reply = await sendCommand(socket, C.TestCurrentTab!, []);
  return String(reply[1]);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("issue-6316: ng only, skipping");
    return;
  }
  const dir = tmpPath("issue-6316");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const doc = join(dir, "paper.pdf");
  writeFileSync(doc, makePdf(2, 612, 0, 0));
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  const logPath = join(dir, "app.log");
  const pipe = `issue-6316-${process.pid}`;
  const args = ["-for-testing", ...windowPosArgs(), "-appdata", dir, "-log-to-file", logPath, "-dbg-control", pipe];
  const proc = Bun.spawn([EXE, ...args, doc], { stdout: "ignore", stderr: "ignore" });
  let socket: Socket | undefined;

  let seen = 0;
  const waitForLog = async (re: RegExp, what: string): Promise<void> => {
    const deadline = Date.now() + kWaitMs;
    for (;;) {
      let log = "";
      try {
        log = readFileSync(logPath, "utf8");
      } catch {
        // not created yet
      }
      if (re.test(log.slice(seen))) {
        seen = log.length;
        return;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-6316: ${what}`);
      }
      await sleep(40);
    }
  };

  try {
    socket = await connect(pipe, kWaitMs);
    await waitForLog(/LoadDocument: .* 2 pages for /, "document never loaded");
    const loaded = await currentTab(socket);
    if (!loaded.endsWith("page=1")) {
      throw new Error(`issue-6316: unexpected tab after load: ${loaded}`);
    }

    // a writer truncates the file and stalls: the auto-reload finds no document
    writeFileSync(doc, "");
    await waitForLog(/ReloadDocument: .*auto refresh: 1/, "no auto-reload of the truncated file");
    const afterBroken = await currentTab(socket);
    if (afterBroken !== loaded) {
      throw new Error(`issue-6316: failed auto-reload dropped the document: ${afterBroken}`);
    }

    // the writer finishes: the next change must be picked up
    writeFileSync(doc, makePdf(3, 612, 0, 0));
    await waitForLog(/LoadDocument: .* 3 pages for /, "finished file was not reloaded");
    const afterFixed = await currentTab(socket);
    if (afterFixed !== loaded) {
      throw new Error(`issue-6316: unexpected tab after recovery: ${afterFixed}`);
    }
  } finally {
    socket?.destroy();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
