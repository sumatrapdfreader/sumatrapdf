// Every toggle command (toggleCommands in cmd/gen-commands.ts) takes an optional
// bool `state` argument: [CmdToggleX on] turns it on and does nothing when it is
// already on. Without it the command toggles.
import { writeFileSync } from "node:fs";
import { toggleCommands } from "../cmd/gen-commands";
import type { ControlClient } from "./control";
import { ControlCommand, withControlledSumatra } from "./control";
import { makeTextPdf } from "./issue-4684";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util";
import { FRAME_CLASS } from "./win-automation";
import { sendCopyDataW, sleep, waitForTopWindow } from "./winapi";

const kCopyDataDdeW = 0x44646557;

// toggles of orig-only features: ng has the command id but not the feature
const kOrigOnly = new Set(["CmdTogglePdfPreviewLogging", "CmdToggleWindowsPreviewer", "CmdToggleWindowsSearchFilter"]);

// toggles whose state exists without a document too, so they're safe to flip here
const kFlipped = [
  "CmdToggleLinks",
  "CmdToggleGrayscale",
  "CmdToggleMirror",
  "CmdToggleMangaMode",
  "CmdToggleContinuousView",
  "CmdTogglePageBoxes",
  "CmdToggleToolbar",
  "CmdToggleThumbnails",
  "CmdFindToggleMatchCase",
  "CmdInvertColors",
  "CmdSelectTextViaKeyboard",
];
// the name swallows the rest of the line, so state goes first:
// [CmdToggleBoolSetting state=on ShowLinks]
const kBoolSettingName = "ShowLinks";
const kBoolSetting = `CmdToggleBoolSetting ${kBoolSettingName}`;

type ToggleInfo = { hasState: boolean; known: boolean; on: boolean };

async function toggleInfo(client: ControlClient, def: string): Promise<ToggleInfo> {
  const [code, raw] = await client.request(ControlCommand.TestToggleCmd, [def]);
  const m = /^OK hasState=(\d) known=(\d) on=(\d)/.exec(String(raw ?? ""));
  if (code !== 0 || !m) throw new Error(`toggle-state-arg: TestToggleCmd '${def}': ${raw}`);
  return { hasState: m[1] === "1", known: m[2] === "1", on: m[3] === "1" };
}

async function expectOn(client: ControlClient, cmd: string, want: boolean, what: string): Promise<void> {
  const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
  let got: ToggleInfo;
  do {
    await client.waitForRenderIdle();
    got = await toggleInfo(client, cmd);
    if (got.on === want) return;
    await sleep(50);
  } while (Date.now() < deadline);
  throw new Error(`toggle-state-arg: ${cmd} after ${what}: on=${got.on}, want ${want}`);
}

async function send(client: ControlClient, frame: number, cmd: string, state: string): Promise<void> {
  const def = cmd === kBoolSetting ? `CmdToggleBoolSetting state=${state} ${kBoolSettingName}` : `${cmd} ${state}`;
  if (!sendCopyDataW(frame, kCopyDataDdeW, `[${def}]`)) throw new Error(`toggle-state-arg: DDE [${def}] not handled`);
  await client.waitForRenderIdle();
}

async function run(client: ControlClient, frame: number): Promise<void> {
  await client.waitForRenderIdle();
  await client.setNotificationsEnabled(false);

  // every toggle command keeps its state argument and knows its state
  const bad: string[] = [];
  for (const c of toggleCommands) {
    const def = c === "CmdToggleBoolSetting" ? `CmdToggleBoolSetting state=on ${kBoolSettingName}` : `${c} on`;
    const info = await toggleInfo(client, def);
    const knownOk = info.known || (USE_NG && kOrigOnly.has(c));
    if (!info.hasState || !knownOk) bad.push(`${c}: hasState=${info.hasState} known=${info.known}`);
  }
  if (bad.length > 0) throw new Error(`toggle-state-arg:\n${bad.join("\n")}`);

  // on, on, off, off: an explicit state is set, never flipped
  for (const c of [...kFlipped, kBoolSetting]) {
    const start = (await toggleInfo(client, c)).on;
    for (const want of [!start, !start, start, start]) {
      await send(client, frame, c, want ? "on" : "off");
      await expectOn(client, c, want, want ? "on" : "off");
    }
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("toggle-state-arg.pdf");
  writeFileSync(pdf, makeTextPdf());
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) throw new Error("toggle-state-arg: no frame window");
      await run(client, frame);
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
