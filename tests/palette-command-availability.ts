// Command palette must omit commands that cannot run in the current context
// (Disable on the menu, Hide on the palette). Context-menu-only copy/show
// commands have no page element after palette dispatch, so they stay hidden.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { postChar, postMessage, sleep, VK_END, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

function makePdf(): string {
  const stream = "BT /F1 24 Tf 72 720 Td (selectme here) Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R " +
      "/Resources << /Font << /F1 5 0 R >> >> >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
  ]);
}

async function vis(client: ControlClient, cmd: string, surface: "palette" | "menu"): Promise<string> {
  const res = await client.request(ControlCommand.TestCommandVisibility, [cmd, surface]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`palette-command-availability: ${cmd} ${surface}: ${raw.trim()}`);
  }
  return /vis=(\w+)/.exec(raw)?.[1] ?? "";
}

async function expectVis(client: ControlClient, cmd: string, surface: "palette" | "menu", want: string): Promise<void> {
  const got = await vis(client, cmd, surface);
  if (got !== want) {
    throw new Error(`palette-command-availability: ${cmd} ${surface} vis=${got}, want ${want}`);
  }
}

async function selectLineWithKeyboard(client: ControlClient, frame: number): Promise<void> {
  const deadline = Date.now() + 4_000 * SLOW_BUILD_FACTOR;
  sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
  let dump = "";
  while (Date.now() < deadline) {
    dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
    if (/active=1/.test(dump)) {
      break;
    }
    await sleep(25);
  }
  if (!/active=1/.test(dump)) {
    throw new Error(`palette-command-availability: keyboard selection did not start\n${dump}`);
  }
  await postChar(frame, "v");
  while (Date.now() < deadline) {
    dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
    if (/visual=1/.test(dump)) {
      break;
    }
    await sleep(25);
  }
  if (!/visual=1/.test(dump)) {
    throw new Error(`palette-command-availability: visual mode did not start\n${dump}`);
  }
  postMessage(frame, WM_KEYDOWN, VK_END, 0);
  postMessage(frame, WM_KEYUP, VK_END, 0);
  await sleep(200);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("palette-command-availability");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
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
    await client.setNotificationsEnabled(false);

    const paletteHide = [
      "CmdCopyAnnotation",
      "CmdCutAnnotation",
      "CmdPasteAnnotation",
      "CmdUndo",
      "CmdRedo",
      "CmdSaveAnnotations",
      "CmdSaveAnnotationsNewFile",
      "CmdDiscardChanges",
      "CmdShowErrors",
      "CmdCopyLinkTarget",
      "CmdCopyComment",
      "CmdShowAnnotationText",
      "CmdCopyImage",
      "CmdFixDefaultApp",
      "CmdInstallPrereleaseUpdate",
      "CmdExpandToCurrentPage",
      "CmdSearchGoogleLens",
      "CmdGoToNextFavorite",
      "CmdGoToPrevFavorite",
      "CmdShowAnnotations",
    ];
    for (const cmd of paletteHide) {
      await expectVis(client, cmd, "palette", "hide");
    }
    await expectVis(client, "CmdHideAnnotations", "palette", "show");
    await expectVis(client, "CmdToggleShowAnnotations", "palette", "show");

    await expectVis(client, "CmdUndo", "menu", "disable");
    await expectVis(client, "CmdCopyAnnotation", "menu", "disable");
    await expectVis(client, "CmdPasteAnnotation", "menu", "disable");
    await expectVis(client, "CmdSaveAnnotations", "menu", "disable");
    await expectVis(client, "CmdShowAnnotations", "menu", "disable");
    await expectVis(client, "CmdHideAnnotations", "menu", "show");

    await selectLineWithKeyboard(client, frame);
    await expectVis(client, "CmdSearchGoogleLens", "palette", "show");

    sendCommandSync(frame, cmdId("CmdCreateAnnotHighlight"));
    const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    let annots = 0;
    let raw = "";
    while (Date.now() < deadline) {
      raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
      annots = +(/annotations=(\d+)/.exec(raw)?.[1] ?? 0);
      if (annots === 1) {
        break;
      }
      await sleep(40);
    }
    if (annots !== 1) {
      throw new Error(`palette-command-availability: highlight was not created\n${raw}`);
    }
    await expectVis(client, "CmdUndo", "palette", "show");
    await expectVis(client, "CmdSaveAnnotations", "palette", "show");
    await expectVis(client, "CmdDiscardChanges", "palette", "show");
    await expectVis(client, "CmdCopyAnnotation", "palette", "hide");
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("palette-command-availability: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
