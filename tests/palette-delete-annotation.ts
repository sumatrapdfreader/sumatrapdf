// CmdDeleteAnnotation is in the command palette only when the cursor is over
// an annotation. Disabled items are hidden on that surface, so it must not
// appear as a grayed row for a click on empty page.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { getClientRect, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [200 380 420 430] " +
      "/QuadPoints [200 430 420 430 200 380 420 380] /C [1 1 0] >>",
  ]);
}

async function vis(client: ControlClient, x: number, y: number): Promise<string> {
  const res = await client.request(ControlCommand.TestCommandVisibility, ["CmdDeleteAnnotation", "palette", x, y]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`palette-delete-annotation: ${raw.trim()}`);
  }
  return /vis=(\w+)/.exec(raw)?.[1] ?? "";
}

async function annotScreen(client: ControlClient): Promise<{ x: number; y: number; dx: number; dy: number }> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const m = /screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    if (res[0] === 0 && m && +m[3]! > 0 && +m[4]! > 0) {
      return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
    }
    if (Date.now() > deadline) {
      throw new Error(`palette-delete-annotation: annotation never loaded\n${raw}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("palette-delete-annotation");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annot.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    dir,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const screen = await annotScreen(client);
    const cx = screen.x + Math.floor(screen.dx / 2);
    const cy = screen.y + Math.floor(screen.dy / 2);
    const over = await vis(client, cx, cy);
    if (over !== "show") {
      throw new Error(`palette-delete-annotation: over annot vis=${over}, want show`);
    }

    const cr = getClientRect(findCanvas(frame));
    const emptyX = Math.max(5, cr.right - 10);
    const emptyY = Math.max(5, cr.bottom - 10);
    const away = await vis(client, emptyX, emptyY);
    if (away !== "hide") {
      throw new Error(`palette-delete-annotation: empty page vis=${away}, want hide`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("palette-delete-annotation: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
