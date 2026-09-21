// #6230: Back after a find-as-you-type search returns to where the search
// started, not to the page of an intermediate match.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, sendCommand, waitForFocusClass, killAndWait } from "./win-automation.ts";
import { sendMessage, WM_CHAR } from "./winapi.ts";

// "x" matches on page 40 and page 80, "xy" only on page 80. Page filler text
// avoids the letters x and y so each search moves the view.
const firstMatchPage = 40;
const secondMatchPage = 80;
const pageCount = 100;

function buildPdf(): Buffer {
  const objects: string[] = [];
  const fontObject = 3;
  objects[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objects[fontObject] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";

  const pages: number[] = [];
  let objectNumber = 4;
  for (let page = 1; page <= pageCount; page++) {
    const pageObject = objectNumber++;
    const contentObject = objectNumber++;
    pages.push(pageObject);
    let text = `Sheet ${page}: no match`;
    if (page === firstMatchPage) {
      text = `Sheet ${page}: x`;
    } else if (page === secondMatchPage) {
      text = `Sheet ${page}: xy`;
    }
    const content = `BT /F1 24 Tf 72 720 Td (${text}) Tj ET`;
    objects[pageObject] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 ${fontObject} 0 R >> >> /Contents ${contentObject} 0 R >>`;
    objects[contentObject] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  }
  objects[2] = `<< /Type /Pages /Kids [${pages.map((page) => `${page} 0 R`).join(" ")}] /Count ${pages.length} >>`;

  let pdf = "%PDF-1.5\n";
  const offsets: number[] = [];
  for (let i = 1; i < objectNumber; i++) {
    offsets.push(Buffer.byteLength(pdf, "latin1"));
    pdf += `${i} 0 obj\n${objects[i]}\nendobj\n`;
  }
  const xrefOffset = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objectNumber}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objectNumber} /Root 1 0 R >>\nstartxref\n${xrefOffset}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function currentPage(client: ControlClient): Promise<number> {
  const response = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const raw = String(response[1] ?? "");
  const match = /OK page=(\d+)/.exec(raw);
  if (!match) {
    throw new Error(`issue-6230: can't read current page: ${raw.trim()}`);
  }
  return Number(match[1]);
}

async function waitForPage(client: ControlClient, expected: number, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let page = 0;
  while (Date.now() < deadline) {
    page = await currentPage(client);
    if (page === expected) {
      return page;
    }
    await Bun.sleep(50);
  }
  return page;
}

async function expectPage(client: ControlClient, expected: number, what: string): Promise<void> {
  const page = await waitForPage(client, expected);
  if (page !== expected) {
    throw new Error(`issue-6230: ${what} landed on page ${page}, want ${expected}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6230");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-6230.pdf");
  writeFileSync(pdf, buildPdf());
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberStatePerDocument = false\nSearchUIFloating = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();

    // start the search from page 5 so Back has somewhere distinct to return to
    const startPage = 5;
    await client.request(ControlCommand.TestFavoriteNav, ["goto", startPage]);
    await expectPage(client, startPage, "go to start page");

    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit = await waitForFocusClass(frame, "Edit");
    // typed with a pause so both incremental searches run and move the view
    sendMessage(edit, WM_CHAR, "x".charCodeAt(0), 0);
    await expectPage(client, firstMatchPage, "search for 'x'");
    sendMessage(edit, WM_CHAR, "y".charCodeAt(0), 0);
    await expectPage(client, secondMatchPage, "search for 'xy'");

    sendCommand(frame, cmdId("CmdNavigateBack"));
    await expectPage(client, startPage, "Back after search");

    sendCommand(frame, cmdId("CmdNavigateForward"));
    await expectPage(client, secondMatchPage, "Forward after Back");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
