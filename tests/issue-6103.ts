// File attachment property row has Attach File and Save Attachment (issue #6103).

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, assemblePdf } from "./util.ts";
import { packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

function makeBlankPdf(): string {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  return assemblePdf(objects);
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const m = /annotEditToolbar .*/.exec(raw);
    if (res[0] === 0 && m && /visible=1/.test(m[0]!)) {
      return m[0]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6103: annotation toolbar did not appear\n${raw}`);
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6103");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
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
    findCanvas(frame);
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFileAttachment"), packCoords(150, 300));
    await sleep(400);
    await client.waitForRenderIdle();

    const dump = await toolbarDump(client);
    if (!/items=/.test(dump) || !/attachFile/.test(dump) || !/saveAttachment/.test(dump) || !/icon/.test(dump)) {
      throw new Error(`issue-6103: file attachment toolbar missing attach/save/icon chips\n${dump}`);
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
