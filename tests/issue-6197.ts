// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6197
//
// Annotations.HighlightColor applies to `a` and to Shift+A
// ("CmdCreateAnnotHighlight openedit"). A command's own color still wins.
//
// Run: bun tests/issue-6197.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { postChar, postMessage, sleep, VK_END, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

async function markupDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`issue-6197: could not read annotations\n${raw}`);
  }
  return raw;
}

function highlightColors(raw: string): string[] {
  return [...raw.matchAll(/type=Highlight page=\d+ quads=\d+\ncolor=#?([0-9a-fA-F]+)/g)].map((m) =>
    m[1]!.toLowerCase().slice(-6),
  );
}

async function selectLineWithKeyboard(client: ControlClient, frame: number): Promise<void> {
  const deadline = Date.now() + 4_000 * SLOW_BUILD_FACTOR;
  const waitFor = async (re: RegExp) => {
    let dump = "";
    while (Date.now() < deadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (re.test(dump)) {
        return;
      }
      await sleep(25);
    }
    throw new Error(`issue-6197: keyboard selection did not reach ${re}\n${dump}`);
  };
  sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
  await waitFor(/active=1/);
  await postChar(frame, "v");
  await waitFor(/visual=1/);
  postMessage(frame, WM_KEYDOWN, VK_END, 0);
  postMessage(frame, WM_KEYUP, VK_END, 0);
  await sleep(200);
}

async function waitForHighlights(client: ControlClient, n: number): Promise<string[]> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let raw = await markupDump(client);
  while (highlightColors(raw).length < n && Date.now() < deadline) {
    await sleep(40);
    raw = await markupDump(client);
  }
  const colors = highlightColors(raw);
  if (colors.length < n) {
    throw new Error(`issue-6197: want ${n} highlights, got ${colors.length}\n${raw}`);
  }
  return colors;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6197");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "Annotations [",
      "\tHighlightColor = #ff0000",
      "]",
      "",
    ].join("\n"),
  );
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // `a`, then a command with its own color, then Shift+A (openedit) last:
    // it opens Edit PDF and the Contents editor
    const commands = ["CmdCreateAnnotHighlight", "CmdCreateAnnotHighlight #00ff00", "CmdCreateAnnotHighlight openedit"];
    for (let i = 0; i < commands.length; i++) {
      await selectLineWithKeyboard(client, frame);
      const res = await client.request(ControlCommand.TestInvokeCommand, [commands[i]!]);
      if (res[0] !== 0) {
        throw new Error(`issue-6197: ${commands[i]}: ${String(res[1] ?? "")}`);
      }
      await client.waitForRenderIdle();
      await waitForHighlights(client, i + 1);
    }

    const colors = (await waitForHighlights(client, commands.length)).sort();
    const want = ["00ff00", "ff0000", "ff0000"];
    if (colors.join(" ") !== want.join(" ")) {
      throw new Error(`issue-6197: highlight colors are ${colors.join(" ")}, want ${want.join(" ")}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6197: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
