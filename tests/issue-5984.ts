// #5984: reopening Find and rapidly retyping the same query must restart at
// the first match at/after the current page, not continue from the old match.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { launchControlled, pressEscape, sendCommand, waitForFocusClass, killAndWait } from "./win-automation.ts";
import { getControlText, sendMessage, WM_CHAR } from "./winapi.ts";

const query = "needle";
const firstMatchPage = 80;
const secondMatchPage = 90;
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
    const hasMatch = page === firstMatchPage || page === secondMatchPage;
    const text = hasMatch ? `Page ${page}: ${query}` : `Page ${page}: no match`;
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
    throw new Error(`issue-5984: can't read current page: ${raw.trim()}`);
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

function typeRapidly(edit: number, text: string): void {
  for (const ch of text) {
    sendMessage(edit, WM_CHAR, ch.charCodeAt(0), 0);
  }
}

const VK_ESCAPE = 0x1b;

async function macFindState(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestToolWindow, ["state", "find"]);
  return String(res[1] ?? "");
}

// The floating find box is the "find" tool window. There is no Edit HWND.
async function waitMacFindEdit(client: ControlClient, selectAll: boolean): Promise<void> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await macFindState(client);
    const text = /editText='([^']*)'/.exec(raw)?.[1];
    const sel = /sel=(-?\d+),(-?\d+)/.exec(raw);
    const focused = / edit=1/.test(raw);
    const all = !!text && !!sel && Number(sel[1]) === 0 && Number(sel[2]) === text.length;
    if (focused && text !== undefined && (!selectAll || all)) {
      return;
    }
    await Bun.sleep(40);
  }
  throw new Error(`issue-5984: focus did not move to the find edit (${raw})`);
}

async function macType(client: ControlClient, text: string): Promise<void> {
  for (const ch of text) {
    const res = await client.request(ControlCommand.TestToolWindow, [
      "input",
      "find",
      "char",
      ch.charCodeAt(0),
      0,
      0,
      0,
    ]);
    const raw = String(res[1] ?? "");
    if (raw.startsWith("ERR") || raw === "NOTREADY") {
      throw new Error(`issue-5984: char '${ch}': ${raw}`);
    }
  }
}

async function macFindText(client: ControlClient): Promise<string> {
  const raw = await macFindState(client);
  return /editText='([^']*)'/.exec(raw)?.[1] ?? "";
}

async function macHideFind(client: ControlClient): Promise<void> {
  const key = await client.request(ControlCommand.TestToolWindow, ["input", "find", "key", VK_ESCAPE, 0, 0, 0]);
  const keyRaw = String(key[1] ?? "");
  if (keyRaw.startsWith("ERR")) {
    throw new Error(`issue-5984: escape: ${keyRaw}`);
  }
  const deadline = Date.now() + 2000 * SLOW_BUILD_FACTOR;
  let last = "";
  while (Date.now() < deadline) {
    last = String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "");
    if (/floating=0/.test(last)) {
      return;
    }
    await Bun.sleep(40);
  }
  throw new Error(`issue-5984: find did not close (${last.trim()})`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5984");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-5984.pdf");
  writeFileSync(pdf, buildPdf());
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberStatePerDocument = false\nSearchUIFloating = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();

    sendCommand(frame, cmdId("CmdFindFirst"));
    let edit = 0;
    if (IS_MAC) {
      await waitMacFindEdit(client, false);
      await macType(client, query);
    } else {
      edit = await waitForFocusClass(frame, "Edit");
      typeRapidly(edit, query);
    }
    let page = await waitForPage(client, firstMatchPage);
    if (page !== firstMatchPage) {
      throw new Error(`issue-5984: initial search landed on page ${page}, want ${firstMatchPage}`);
    }

    if (IS_MAC) {
      await macHideFind(client);
    } else {
      await pressEscape(edit);
    }
    await client.request(ControlCommand.TestFavoriteNav, ["goto", 1]);
    page = await waitForPage(client, 1);
    if (page !== 1) {
      throw new Error(`issue-5984: navigation landed on page ${page}, want 1`);
    }

    sendCommand(frame, cmdId("CmdFindFirst"));
    let repeatedText = "";
    if (IS_MAC) {
      await waitMacFindEdit(client, true);
      await macType(client, query);
      repeatedText = await macFindText(client);
    } else {
      edit = await waitForFocusClass(frame, "Edit");
      typeRapidly(edit, query);
      repeatedText = getControlText(edit);
    }
    if (repeatedText !== query) {
      throw new Error(`issue-5984: repeated typing produced '${repeatedText}', want '${query}'`);
    }
    page = await waitForPage(client, firstMatchPage);
    if (page !== firstMatchPage) {
      throw new Error(
        `issue-5984: repeated search landed on page ${page}, want first result on page ${firstMatchPage}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
