// The color chip of a selected text markup annotation opens the same color
// drop-down the Edit PDF toolbar's markup buttons have, and the color it
// applies carries its own opacity, so those annotations have no opacity chip.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { findTopWindow, getWindowRect, isWindowVisible, sleep } from "./winapi.ts";
import { clickAt, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const TOOLBAR_CLASS = "SumatraAnnotEditToolbar";
const POPUP_CLASS = "SumatraAnnotColorPopup";
// the two preset colors the test picks from: translucent red, opaque green
const PRESETS = "#80ff0000 #00ff00";
const PICKED_COLOR = "#ff0000";
const PICKED_OPACITY = 0x80;

type Rect = { x: number; y: number; dx: number; dy: number };

function parseRect(m: RegExpExecArray | null): Rect {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /annotEditToolbar .*/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`annot-color-dropdown: could not read toolbar state\n${raw}`);
  }
  return m[0]!;
}

// the selected annotation's color and opacity
async function selectedColor(client: ControlClient): Promise<{ color: string; opacity: number }> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / color=(\S+) opacity=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`annot-color-dropdown: could not read annotation color: ${raw}`);
  }
  return { color: m[1]!, opacity: +m[2]! };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-color-dropdown");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const nl = String.fromCharCode(10);
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "Annotations [",
      `\tPresetColors = ${PRESETS}`,
      "]",
      "",
    ].join(nl),
  );

  const pdf = join(process.cwd(), "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(400);

    // a highlight over the text of page 1, which leaves it selected
    await client.seedTextSelection(1);
    sendCommand(frame, cmdId("CmdCreateAnnotHighlight"));
    await sleep(600);
    await client.waitForRenderIdle();

    const dump = await toolbarDump(client);
    if (!/annotEditToolbar visible=1/.test(dump)) {
      throw new Error(`annot-color-dropdown: no annotation toolbar: ${dump}`);
    }
    const items = / items=(\S+)/.exec(dump)?.[1] ?? "";
    if (!items.split(",").includes("color")) {
      throw new Error(`annot-color-dropdown: no color chip: ${dump}`);
    }
    if (items.split(",").includes("opacity")) {
      throw new Error(`annot-color-dropdown: the color chip did not replace the opacity chip: ${dump}`);
    }

    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const chip = parseRect(/[=;]color:(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const tbHwnd = findTopWindow(proc.pid!, TOOLBAR_CLASS);
    if (!tbHwnd) {
      throw new Error("annot-color-dropdown: annotation toolbar window not found");
    }
    await clickAt(tbHwnd, chip.x - placed.x + Math.floor(chip.dx / 2), chip.y - placed.y + Math.floor(chip.dy / 2));
    await sleep(500);

    const popup = findTopWindow(proc.pid!, POPUP_CLASS);
    if (!popup || !isWindowVisible(popup)) {
      throw new Error("annot-color-dropdown: the color drop-down did not open");
    }
    const pr = getWindowRect(popup);

    // the swatches are the bottom row, the first one at its left edge
    const swatchDy = pr.bottom - pr.top;
    const cx = Math.floor((pr.right - pr.left) / 10);
    const cy = swatchDy - Math.floor(swatchDy / 5);
    await clickAt(popup, cx, cy);
    await sleep(600);
    await client.waitForRenderIdle();

    const stillUp = findTopWindow(proc.pid!, POPUP_CLASS);
    if (stillUp && isWindowVisible(stillUp)) {
      throw new Error("annot-color-dropdown: the drop-down stayed up after picking a color");
    }
    const got = await selectedColor(client);
    if (got.color !== PICKED_COLOR || got.opacity !== PICKED_OPACITY) {
      throw new Error(
        `annot-color-dropdown: annotation is ${got.color}/${got.opacity}, want ${PICKED_COLOR}/${PICKED_OPACITY}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-color-dropdown: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
