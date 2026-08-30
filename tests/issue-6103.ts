// File attachment property row: Attach File always, Save Attachment once a
// file is embedded (issue #6103).

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { findTopWindow, getWindowPid, packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi.ts";
import { clickAt, killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

const FLOAT_CLASS = "SUMATRA_ANNOT_FILTER_WND";

function makePdfWithAttachment(): string {
  const payload = "hello";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /FileAttachment /P 3 0 R /Rect [298 388 314 404] /FS 5 0 R /Name /PushPin >>",
    "<< /Type /Filespec /F (hello.txt) /UF (hello.txt) /EF << /F 6 0 R >> >>",
    `<< /Type /EmbeddedFile /Length ${payload.length} /Params << /Size ${payload.length} >> >>\nstream\n${payload}\nendstream`,
  ]);
}

async function markupDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  return String(res[1] ?? "");
}

async function toolbarDump(client: ControlClient, what: string): Promise<string> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  let raw = "";
  for (;;) {
    raw = await markupDump(client);
    const m = /annotEditToolbar .*/.exec(raw);
    if (m && /visible=1/.test(m[0]!)) {
      return m[0]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6103: ${what}\n${raw}`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6103");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "attach.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdfWithAttachment(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

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
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    sendCommandSync(frame, cmdId("CmdFindAnnotation"));
    const listDeadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    let listRaw = "";
    let itemDy = 0;
    let listY = 0;
    for (;;) {
      listRaw = await markupDump(client);
      const nAll = +(/nAll=(\d+)/.exec(listRaw)?.[1] ?? 0);
      itemDy = +(/itemDy=(-?\d+)/.exec(listRaw)?.[1] ?? 0);
      listY = +(/listY=(-?\d+)/.exec(listRaw)?.[1] ?? 0);
      if (/annotFilter floatVisible=1/.test(listRaw) && nAll >= 1 && itemDy > 0) {
        break;
      }
      if (Date.now() > listDeadline) {
        throw new Error(`issue-6103: annotation list did not open\n${listRaw}`);
      }
      await sleep(40);
    }
    const pid = getWindowPid(frame) || proc.pid!;
    const floatWnd = findTopWindow(pid, FLOAT_CLASS);
    if (!floatWnd) {
      throw new Error(`issue-6103: annotation list window missing\n${listRaw}`);
    }
    await clickAt(floatWnd, 24, listY + Math.floor(itemDy / 2));

    const withFile = await toolbarDump(client, "file attachment toolbar did not appear");
    if (!/attachFile/.test(withFile) || !/saveAttachment/.test(withFile) || !/icon/.test(withFile)) {
      throw new Error(`issue-6103: file attachment toolbar missing attach/save/icon chips\n${withFile}`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFileAttachment"), packCoords(40, 40));
    await sleep(400);
    await client.waitForRenderIdle();

    const empty = await toolbarDump(client, "empty file attachment toolbar did not appear");
    if (!/attachFile/.test(empty) || !/icon/.test(empty)) {
      throw new Error(`issue-6103: empty file attachment toolbar missing attach/icon chips\n${empty}`);
    }
    if (/saveAttachment/.test(empty)) {
      throw new Error(`issue-6103: Save Attachment shown before a file is embedded\n${empty}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6103: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
