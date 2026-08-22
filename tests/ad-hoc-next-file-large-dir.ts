// Repro for discussion #6014: last-page / next-file in a folder of 50k PDFs.
// Uses ~\Downloads\sumatra-next-file-50k (created by tests/tmp/make-next-file-50k.ts).
// Not registered in run-almost-all: the folder is huge and local-only.
import { existsSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util";
import { ControlCommand } from "./control";
import { getWindowText, postMessage, sleep, WM_CLOSE } from "./winapi";
import { killAndWait, launchControlled, sendCommandSync, waitForExit, waitForTitle } from "./win-automation";

const DIR = join(homedir(), "Downloads", "sumatra-next-file-50k");
const FIRST = join(DIR, "f-00001.pdf");

export async function testit(): Promise<void> {
  if (!existsSync(FIRST)) {
    console.log(`skip: ${DIR} is missing (run bun tests/tmp/make-next-file-50k.ts)`);
    return;
  }
  const logPath = tmpPath("next-file-50k.log");
  const { proc, client, frame } = await launchControlled(["-log", "-log-to-file", logPath, FIRST]);
  try {
    await waitForTitle(frame, (t) => t.includes("f-00001.pdf"), 20000);
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    const tPing0 = Date.now();
    await client.request(ControlCommand.Ping);
    const ping0 = Date.now() - tPing0;
    console.log(`ping after load: ${ping0}ms`);

    const tLast = Date.now();
    sendCommandSync(frame, cmdId("CmdGoToLastPage"));
    const lastMs = Date.now() - tLast;
    console.log(`CmdGoToLastPage (SendMessage, includes next-file hint): ${lastMs}ms`);

    const tPing1 = Date.now();
    await client.request(ControlCommand.Ping);
    const ping1 = Date.now() - tPing1;
    console.log(`ping after last page: ${ping1}ms`);

    const tNext = Date.now();
    sendCommandSync(frame, cmdId("CmdOpenNextFileInFolder"));
    const nextCmdMs = Date.now() - tNext;
    console.log(`CmdOpenNextFileInFolder (SendMessage): ${nextCmdMs}ms`);

    await waitForTitle(frame, (t) => t.includes("f-00002.pdf"), 30000);
    const nextTitleMs = Date.now() - tNext;
    console.log(`next file title f-00002.pdf after ${nextTitleMs}ms (now '${getWindowText(frame)}')`);

    const tNav = Date.now();
    sendCommandSync(frame, cmdId("CmdNavigateFilesInFolder"));
    const navMs = Date.now() - tNav;
    console.log(`CmdNavigateFilesInFolder (SendMessage): ${navMs}ms`);
    let pingMax = 0;
    while (Date.now() - tNav < 6000) {
      const t = Date.now();
      await client.request(ControlCommand.Ping);
      pingMax = Math.max(pingMax, Date.now() - t);
      await sleep(200);
    }
    console.log(`max ping during navigate-files listing: ${pingMax}ms`);

    postMessage(frame, WM_CLOSE, 0, 0);
    await waitForExit(proc, 8000);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  if (existsSync(logPath)) {
    const log = readFileSync(logPath, "utf8");
    const interesting = log
      .split(/\r?\n/)
      .filter((l) => /CollectNextPrev|OpenNextPrev|HangDetector|UI thread blocked|NextPrevDir|NavDirScan/i.test(l));
    console.log("--- log ---");
    console.log(interesting.join("\n") || "(no matching lines)");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
