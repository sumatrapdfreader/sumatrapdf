// PR #6316: ng replaced the document with an error page when an auto-reload
// hit a file caught mid-rewrite, and then never reloaded it again. Like orig,
// a failed auto-reload must keep the document and retry on the next change.
// A tab that does show the error page (a manual reload of the broken file)
// recovers on the next file change, or when the file is opened again.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { Socket } from "node:net";
import { connect, controlCommands as C, sendCommand } from "../cmd/ng-dbg-control";
import { EXE, USE_NG, makePdf, runStandalone, tmpPath } from "./util";
import { windowPosArgs } from "./win-automation";
import { killAndWait, sleep } from "./winapi";

const kWaitMs = 15000;

type Mode = "auto" | "reopen";

async function runMode(mode: Mode): Promise<void> {
  const dir = tmpPath(`issue-6316-${mode}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const doc = join(dir, "paper.pdf");
  writeFileSync(doc, makePdf(2, 612, 0, 0));
  const settings = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
ReloadModifiedDocuments = ${mode === "auto"}
`;
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings);

  const logPath = join(dir, "app.log");
  const pipe = `issue-6316-${process.pid}-${mode}`;
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
        throw new Error(`issue-6316 (${mode}): ${what}`);
      }
      await sleep(40);
    }
  };
  const expectTab = async (want: string, what: string): Promise<void> => {
    const reply = await sendCommand(socket!, C.TestCurrentTab!, []);
    const got = String(reply[1]);
    if (!got.endsWith(want)) {
      throw new Error(`issue-6316 (${mode}): ${what}: ${got}`);
    }
  };

  try {
    socket = await connect(pipe, kWaitMs);
    await waitForLog(/LoadDocument: .* 2 pages for /, "document never loaded");
    await expectTab("page=1", "unexpected tab after load");

    // a writer truncates the file and stalls
    writeFileSync(doc, "");

    if (mode === "auto") {
      // the auto-reload finds no document and keeps the one on screen
      await waitForLog(/ReloadDocument: .*auto refresh: 1/, "no auto-reload of the truncated file");
      await expectTab("page=1", "failed auto-reload dropped the document");

      // the writer finishes: the next change must be picked up
      writeFileSync(doc, makePdf(3, 612, 0, 0));
      await waitForLog(/LoadDocument: .* 3 pages for /, "finished file was not reloaded");
      await expectTab("page=1", "unexpected tab after recovery");

      writeFileSync(doc, "");
      await waitForLog(/ReloadDocument: .*auto refresh: 1/, "no auto-reload of the truncated file");
    }

    // a manual reload of the broken file does show the error page
    await sendCommand(socket, C.TestInvokeCommand!, ["CmdReloadDocument"]);
    await expectTab("page=0", "manual reload of a broken file kept the document");

    writeFileSync(doc, makePdf(4, 612, 0, 0));
    if (mode === "reopen") {
      await sendCommand(socket, C.TestOpenFiles!, [doc]);
    }
    await waitForLog(/LoadDocument: .* 4 pages for /, "error tab did not recover");
    await expectTab("page=1", "unexpected tab after the error tab recovered");
  } finally {
    socket?.destroy();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("issue-6316: ng only, skipping");
    return;
  }
  await runMode("auto");
  await runMode("reopen");
}

if (import.meta.main) {
  await runStandalone(testit);
}
