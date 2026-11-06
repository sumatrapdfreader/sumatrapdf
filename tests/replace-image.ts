// Replace an image embedded in a PDF (the image's context menu): Replace
// Image... draws another image file in its place, and Edit Image in Another
// App replaces it whenever that app saves the image's temporary PNG. Each
// replacement is one undo step, and saving the PDF keeps it.
//
// The page has one red image; the test replaces it with blue, then (as the
// other app) with green, undoes back to blue, saves and reopens.
//
// Run: bun tests/replace-image.ts [--no-build]

import { copyFileSync, writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { assemblePdf, makePng, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  findCanvas,
  findSubMenu,
  killAndWait,
  launchControlled,
  readContextMenuTree,
  waitForExit,
} from "./win-automation.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";

type Color = "red" | "green" | "blue";

function makeImagePdf(): string {
  // 2 x 2 red RGB image
  const pixels = "\xff\x00\x00".repeat(4);
  const content = "q 200 0 0 200 100 50 cm /Im1 Do Q";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 300] /Contents 4 0 R " +
      "/Resources << /XObject << /Im1 5 0 R >> >> >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /XObject /Subtype /Image /Width 2 /Height 2 /ColorSpace /DeviceRGB /BitsPerComponent 8 ` +
      `/Length ${pixels.length} >>\nstream\n${pixels}\nendstream`,
  ]);
}

// the color that covers most of the canvas's strongly colored pixels
function dominantColor(frame: number): Color | null {
  const cap = captureWindowPixels(findCanvas(frame));
  if (!cap) {
    throw new Error("replace-image: could not capture the canvas");
  }
  const counts: Record<Color, number> = { red: 0, green: 0, blue: 0 };
  const { data } = cap;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r > 180 && g < 80 && b < 80) counts.red++;
    else if (g > 180 && r < 80 && b < 80) counts.green++;
    else if (b > 180 && r < 80 && g < 80) counts.blue++;
  }
  const best = (Object.keys(counts) as Color[]).sort((a, b) => counts[b] - counts[a])[0]!;
  return counts[best] > 1000 ? best : null;
}

async function waitColor(client: ControlClient, frame: number, want: Color, what: string): Promise<void> {
  const deadline = Date.now() + 8000;
  let got: Color | null = null;
  while (Date.now() < deadline) {
    await client.waitForRenderIdle();
    got = dominantColor(frame);
    if (got === want) {
      return;
    }
    await sleep(150);
  }
  throw new Error(`replace-image: ${what}: expected the image to be ${want}, it is ${got}`);
}

async function image(client: ControlClient, action: string, path = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestImageReplace, [action, 1, 0, path]);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("replace-image: only in the Windows app");
    return;
  }
  const pdf = tmpPath("replace-image.pdf");
  writeFileSync(pdf, makeImagePdf(), "latin1");
  const blue = tmpPath("replace-image-blue.png");
  writeFileSync(blue, makePng(4, 4, [0, 0, 255]));

  let { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.setNotificationsEnabled(false);
    await waitColor(client, frame, "red", "the document");

    // right-click on the image: both commands are in its submenu
    const menu = await readContextMenuTree(findCanvas(frame));
    const imageMenu = findSubMenu(menu, "Selected Image");
    const labels = (imageMenu?.items ?? []).map((it) => it.text);
    for (const want of ["Replace Image...", "Edit Image in Another App"]) {
      if (!labels.includes(want)) {
        throw new Error(`replace-image: "${want}" missing from the image menu: ${JSON.stringify(labels)}`);
      }
    }

    const res = await image(client, "replace", blue);
    if (!res.includes("ok=1") || !res.includes("dirty=1")) {
      throw new Error(`replace-image: Replace Image failed: ${res}`);
    }
    await waitColor(client, frame, "blue", "after Replace Image");

    // the other app saves its edit over the temporary PNG
    const edit = await image(client, "edit");
    const tmp = /path=(.*)/.exec(edit)?.[1]?.trim();
    if (!tmp) {
      throw new Error(`replace-image: no temporary file to edit: ${edit}`);
    }
    writeFileSync(tmp, makePng(4, 4, [0, 255, 0]));
    await waitColor(client, frame, "green", "after the other app saved");

    await client.request(ControlCommand.TestInvokeCommand, ["CmdUndo"]);
    await waitColor(client, frame, "blue", "after Undo");

    await client.request(ControlCommand.TestInvokeCommand, ["CmdSaveAnnotations"]);
    await sleep(500);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdExit"]);
    if (!(await waitForExit(proc))) {
      throw new Error("replace-image: did not exit");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  // a copy, so the saved file isn't open in the app when the next run rewrites it
  const saved = tmpPath("replace-image-saved.pdf");
  copyFileSync(pdf, saved);
  ({ proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", saved]));
  try {
    await waitColor(client, frame, "blue", "after saving and reopening");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("  replaced, edited, undone, saved: blue");
}

if (import.meta.main) {
  await runStandalone(testit);
}
