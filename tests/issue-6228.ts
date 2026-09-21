// CmdGoToHomePage selects the Home tab, creating it first when NoHomeTab hid it.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

function makePdf(): string {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  return assemblePdf(objects);
}

type Client = { request: (cmd: ControlCommand, args: unknown[]) => Promise<unknown[]> };

// true when the current tab shows a document, false on Home
async function onDocument(client: Client): Promise<boolean> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  return res[0] === 0;
}

function check(cond: boolean, what: string): void {
  if (!cond) {
    throw new Error(`issue-6228: ${what}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6228");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "document.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nUseTabs = true\nNoHomeTab = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    check(await onDocument(client), "document should be the current tab");

    // no Home tab yet: the command creates it
    sendCommandSync(frame, cmdId("CmdGoToHomePage"));
    check(!(await onDocument(client)), "Home should be current after CmdGoToHomePage");

    sendCommandSync(frame, cmdId("CmdNextTab"));
    check(await onDocument(client), "CmdNextTab should return to the document");

    // Home tab exists: the command selects it instead of adding another
    sendCommandSync(frame, cmdId("CmdGoToHomePage"));
    check(!(await onDocument(client)), "Home should be current after second CmdGoToHomePage");

    sendCommandSync(frame, cmdId("CmdClose"));
    check(await onDocument(client), "closing Home should leave the document current");
    // with a duplicate Home tab this would land on it
    sendCommandSync(frame, cmdId("CmdNextTab"));
    check(await onDocument(client), "one Home tab was created, not two");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
