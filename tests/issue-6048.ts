// #6048: custom SVG buttons on the floating selection toolbar use the
// handler Name as a tooltip, use the main ToolbarSize, and support separators.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const PDF = join(ROOT, "tests", "issue-4398.pdf");

function parseLayoutCommands(raw: string): number[] {
  const result: number[] = [];
  for (const line of raw.split("\n")) {
    const m = /^cmd=(-?\d+)$/.exec(line.trim());
    if (m) {
      result.push(+m[1]!);
    }
  }
  return result;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6048-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "RestoreSession = false",
      "CheckForUpdates = false",
      "SelectionToolbar = true",
      "ToolbarSize = 42",
      "SelectionToolbarLayout = | CmdCopySelection Separator CmdTranslateSelection | CmdCreateAnnotHighlight",
      "SelectionHandlers [",
      "    [",
      "        Name = Orange Icon",
      "        Exe = helper.exe ${selection}",
      '        SelectToolbarNameOrSvg = <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" stroke="currentColor" fill="none"><path d="M4 12h16"/></svg>',
      "    ]",
      "]",
      "",
    ].join("\n"),
  );

  const { proc, client } = await launchControlled(["-appdata", appdata, PDF]);
  try {
    await client.waitForRenderIdle();
    const raw = String((await client.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
    const expectedLayout = [
      0,
      cmdId("CmdCopySelection"),
      0,
      cmdId("CmdTranslateSelection"),
      0,
      cmdId("CmdCreateAnnotHighlight"),
    ];
    const layout = parseLayoutCommands(raw);
    if (layout.join(",") !== expectedLayout.join(",")) {
      throw new Error(`issue-6048: separators were not preserved in the parsed layout:\n${raw}`);
    }

    const summary = /^buttons=(\d+) separators=(\d+) toolbarSize=(\d+),(\d+) mainIconSize=(\d+)$/m.exec(raw);
    if (!summary) {
      throw new Error(`issue-6048: missing laid-out toolbar summary:\n${raw}`);
    }
    if (+summary[1]! !== 6 || +summary[2]! !== 2) {
      throw new Error(`issue-6048: expected 4 buttons and 2 normalized separators:\n${raw}`);
    }
    const mainIconSize = +summary[5]!;
    if (mainIconSize < 42) {
      throw new Error(`issue-6048: main icon size did not reflect ToolbarSize=42:\n${raw}`);
    }

    const custom = /^button=\d+ cmd=\d+ kind=icon icon=(\d+),(\d+) tooltip=Orange Icon$/m.exec(raw);
    if (!custom) {
      throw new Error(`issue-6048: custom SVG button did not use Name as its tooltip:\n${raw}`);
    }
    if (+custom[1]! !== mainIconSize || +custom[2]! !== mainIconSize) {
      throw new Error(`issue-6048: custom SVG size did not match the main toolbar icon size:\n${raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
