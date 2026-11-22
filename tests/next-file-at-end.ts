// At the end of a document, going on (next page, Page Down, Space, ...) offers
// the next file in the folder; going on again while the offer shows opens it,
// in reading and presentation modes. ShowFileNavigateHint = false turns both off.
//
// Run: bun tests/next-file-at-end.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, makePdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand, waitForTitle } from "./win-automation.ts";
import { getWindowText, sleep } from "./winapi.ts";

const name = "next-file-at-end";

async function notifMsg(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  return /notifMsg='([^']*)'/.exec(raw)?.[1] ?? "";
}

async function waitOffer(client: ControlClient, what: string): Promise<void> {
  await pollUntil(
    () => notifMsg(client),
    (m) => m.includes("b.pdf"),
    { timeoutMs: 5000 * SLOW_BUILD_FACTOR, error: (m) => `${name}: ${what}: no offer of b.pdf ('${m}')` },
  );
}

async function run(dir: string, presenting: boolean): Promise<void> {
  const { proc, client, frame } = await launchControlled([join(dir, "a.pdf")]);
  try {
    await client.waitForRenderIdle();
    if (presenting) {
      sendCommand(frame, cmdId("CmdTogglePresentationMode"));
      await client.waitForRenderIdle();
    }
    const mode = presenting ? "presenting" : "reading";
    // a.pdf has one page: going on is past its end
    sendCommand(frame, cmdId("CmdGoToNextPage"));
    await waitOffer(client, mode);
    if (!getWindowText(frame).includes("a.pdf")) {
      throw new Error(`${name}: ${mode}: the offer alone left a.pdf`);
    }
    sendCommand(frame, cmdId("CmdScrollDownPage"));
    await waitForTitle(frame, (t) => t.includes("b.pdf"));
    if (presenting) {
      await client.waitForRenderIdle();
      const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
      if (!/ presentation=1/.test(raw)) {
        throw new Error(`${name}: b.pdf opened out of presentation mode: ${raw}`);
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// with the hint turned off, going on twice stays in a.pdf
async function runOff(dir: string): Promise<void> {
  const appData = join(dir, "appdata");
  mkdirSync(appData, { recursive: true });
  writeFileSync(join(appData, "SumatraPDF-settings.txt"), "ShowFileNavigateHint = false\n");
  const { proc, client, frame } = await launchControlled(["-appdata", appData, join(dir, "a.pdf")]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdGoToNextPage"));
    sendCommand(frame, cmdId("CmdGoToNextPage"));
    await sleep(500);
    await client.waitForRenderIdle();
    if (!getWindowText(frame).includes("a.pdf") || (await notifMsg(client)).includes("b.pdf")) {
      throw new Error(`${name}: ShowFileNavigateHint = false still went on: '${getWindowText(frame)}'`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (const f of ["a.pdf", "b.pdf"]) {
    writeFileSync(join(dir, f), makePdf(1, 612, 0, 0), "latin1");
  }
  await run(dir, false);
  await run(dir, true);
  await runOff(dir);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
