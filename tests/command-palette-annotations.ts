// Command Palette `*` lists PDF annotations (same rows/filter as Find Annotation)
// and Enter jumps to the selected one.
//
// Run: bun tests/command-palette-annotations.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { postMessage, sleep, VK_RETURN, WM_KEYDOWN } from "./winapi.ts";
import {
  killAndWait,
  launchControlled,
  sendCommand,
  waitForPalette,
  paletteSetText,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";
const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    "<< /Type /Annot /Subtype /Text /P 3 0 R /Rect [72 700 92 720] /Contents (palette-annot-alpha) >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 500 220 530] " +
      "/QuadPoints [72 530 220 530 72 500 220 500] /C [1 1 0] /Contents (palette-annot-beta) >>",
  ];
  return assemblePdf(objs);
}

async function annotContents(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, []);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("command-palette-annotations");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));

    const openDeadline = Date.now() + 8_000;
    let handles = { palette: 0, edit: 0 };
    while (Date.now() < openDeadline) {
      if (IS_MAC) {
        if ((await paletteState(client)).open) {
          handles = { palette: frame, edit: frame };
          break;
        }
      } else {
        handles = await waitForPalette(client, frame, 50);
        if (handles.palette && handles.edit) {
          break;
        }
      }
      await sleep(50);
    }
    if (!handles.palette || !handles.edit) {
      throw new Error("command-palette-annotations: palette did not open");
    }

    const annotDeadline = Date.now() + 8_000;
    let collected: PaletteState | null = null;
    while (Date.now() < annotDeadline) {
      collected = await paletteState(client);
      if (collected?.open && collected.annots >= 2) {
        break;
      }
      await sleep(40);
    }
    if (!collected?.open || collected.annots < 2) {
      throw new Error(`command-palette-annotations: annotations not collected: ${JSON.stringify(collected)}`);
    }

    const query = "* palette-annot-beta";
    await paletteSetText(client, handles, query);
    const filterDeadline = Date.now() + 3_000;
    let filtered: PaletteState | null = null;
    while (Date.now() < filterDeadline) {
      filtered = await paletteState(client);
      if (filtered?.open && filtered.queryLen === query.length && filtered.items === 1) {
        break;
      }
      await sleep(40);
    }
    if (!filtered?.open || filtered.items !== 1) {
      throw new Error(`command-palette-annotations: filter failed: ${JSON.stringify(filtered)}`);
    }

    postMessage(handles.palette, WM_KEYDOWN, VK_RETURN, 0);
    const closeDeadline = Date.now() + 3_000;
    while (Date.now() < closeDeadline) {
      if (!(await paletteState(client)).open) {
        break;
      }
      await sleep(40);
    }
    if ((await paletteState(client)).open) {
      throw new Error("command-palette-annotations: palette did not close");
    }

    const jumpDeadline = Date.now() + 3_000;
    let dump = "";
    while (Date.now() < jumpDeadline) {
      dump = await annotContents(client);
      if (dump.includes("palette-annot-beta")) {
        break;
      }
      await sleep(40);
    }
    if (!dump.includes("palette-annot-beta")) {
      throw new Error(`command-palette-annotations: did not jump to annotation: ${dump.trim()}`);
    }

    console.log("command-palette-annotations: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
