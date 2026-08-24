// A Favorites tab remains visible after closing the last document tab.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

function makePdf(): string {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objects.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  body += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    body += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  body += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

function requireVisible(name: string, visible: boolean | undefined): void {
  if (!visible) {
    throw new Error(`favorites-tab-after-document-close: ${name} should be visible`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("favorites-tab-after-document-close");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "document.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\nShowFavorites = false\nUseTabs = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const add = await client.request(ControlCommand.TestFavoriteNav, ["add", 1]);
    if (add[0] !== 0) {
      throw new Error(`favorites-tab-after-document-close: could not add favorite: ${String(add[1] ?? "")}`);
    }

    sendCommandSync(frame, cmdId("CmdFavoriteShowInTab"));
    let layout = await client.layout();
    requireVisible("Favorites content", layout.items.favorites?.visible);
    requireVisible("tab bar with document and Favorites tabs", layout.items.tabs?.visible);

    sendCommandSync(frame, cmdId("CmdPrevTab"));
    sendCommandSync(frame, cmdId("CmdClose"));
    layout = await client.layout();
    requireVisible("Favorites content after closing the document", layout.items.favorites?.visible);
    requireVisible("lone Favorites tab after closing the document", layout.items.tabs?.visible);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
