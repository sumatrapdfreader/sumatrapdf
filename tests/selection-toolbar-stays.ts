// Copy on the floating selection toolbar must keep the text selection and
// leave the toolbar visible with it. Underline consumes the selection: it
// turns it into an annotation, so the selection and its toolbar go away and
// the new annotation is selected instead.
//
// Run: bun tests/selection-toolbar-stays.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { WM_CHAR, WM_KEYDOWN, WM_KEYUP, postMessage, sleep } from "./winapi.ts";

const VK_END = 0x23;
const LINE = "The quick brown fox jumps over the lazy dog";

function makeTextPdf(): Buffer {
  const enc = (s: string) => Buffer.from(s, "latin1");
  const body: Record<number, Buffer> = {};
  body[1] = enc("<< /Type /Catalog /Pages 2 0 R >>");
  body[2] = enc("<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
  body[6] = enc("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
  const stream = `BT /F1 18 Tf 72 700 Td (${LINE}) Tj ET`;
  body[10] = enc(`<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`);
  body[3] = enc(
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] ` +
      `/Resources << /Font << /F1 6 0 R >> >> /Contents 10 0 R >>`,
  );
  const maxN = 12;
  const parts: Buffer[] = [enc("%PDF-1.7\n%\xe2\xe3\xcf\xd3\n")];
  const offsets: Record<number, number> = {};
  let pos = parts[0]!.length;
  for (let n = 1; n <= maxN; n++) {
    offsets[n] = pos;
    const obj = Buffer.concat([enc(`${n} 0 obj\n`), body[n] ?? enc("null"), enc("\nendobj\n")]);
    parts.push(obj);
    pos += obj.length;
  }
  let xref = `xref\n0 ${maxN + 1}\n0000000000 65535 f \n`;
  for (let n = 1; n <= maxN; n++) {
    xref += `${String(offsets[n]).padStart(10, "0")} 00000 n \n`;
  }
  parts.push(enc(`${xref}trailer\n<< /Size ${maxN + 1} /Root 1 0 R >>\nstartxref\n${pos}\n%%EOF\n`));
  return Buffer.concat(parts);
}

function writeAppData(dir: string): string {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["RestoreSession = false", "CheckForUpdates = false", "ShowToc = false", "SelectionToolbar = true", ""].join("\n"),
  );
  return dir;
}

function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

function parseExpanded(raw: string): string {
  const m = /^expanded=(.*)$/m.exec(raw);
  if (!m) {
    throw new Error(`selection-toolbar-stays: no expanded= in:\n${raw}`);
  }
  return m[1]!.trim();
}

function toolbarVisible(raw: string): boolean {
  return /^visible=1$/m.test(raw);
}

async function toolbarDump(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestSelectionToolbar, []))[1] ?? "");
}

async function waitForToolbarVisible(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = await toolbarDump(client);
    if (toolbarVisible(raw)) {
      return raw;
    }
    await sleep(40);
  }
  throw new Error(`selection-toolbar-stays: selection toolbar did not appear\n${raw}`);
}

async function selectedText(client: ControlClient): Promise<string> {
  const raw = String((await client.request(ControlCommand.TestSelectionVars, ["${selection}"]))[1] ?? "");
  return parseExpanded(raw);
}

async function waitForSelectionGone(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let got = "";
  while (Date.now() < deadline) {
    got = await selectedText(client);
    if (got === "") {
      return;
    }
    await sleep(25);
  }
  throw new Error(`selection-toolbar-stays: selection is still '${got}' after creating the annotation`);
}

async function waitForSelection(client: ControlClient, want: string): Promise<string> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let got = "";
  let raw = "";
  while (Date.now() < deadline) {
    raw = String((await client.request(ControlCommand.TestSelectionVars, ["${selection}"]))[1] ?? "");
    got = parseExpanded(raw);
    if (got === want) {
      return got;
    }
    await sleep(25);
  }
  throw new Error(`selection-toolbar-stays: selected text is '${got}', want '${want}'\n${raw}`);
}

async function clickToolbar(client: ControlClient, cmdName: string): Promise<void> {
  const res = await client.request(ControlCommand.TestSelectionToolbar, [cmdName]);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !/^OK$/m.test(raw)) {
    throw new Error(`selection-toolbar-stays: click ${cmdName} failed (${res[0]}): ${raw}`);
  }
}

async function waitForUnderline(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let raw = "";
  while (Date.now() < deadline) {
    raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    if (/^type=Underline /m.test(raw)) {
      return;
    }
    await sleep(25);
  }
  throw new Error(`selection-toolbar-stays: underline annotation was not created\n${raw}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("selection-toolbar-stays.pdf");
  writeFileSync(pdf, makeTextPdf());
  const dir = writeAppData(tmpPath("selection-toolbar-stays-appdata"));

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdSelectTextViaKeyboard"));
    const startDeadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
    let dump = "";
    while (Date.now() < startDeadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/active=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/active=1/.test(dump)) {
      throw new Error(`selection-toolbar-stays: keyboard selection did not start\n${dump}`);
    }

    postMessage(frame, WM_CHAR, "v".charCodeAt(0), 0);
    while (Date.now() < startDeadline) {
      dump = String((await client.request(ControlCommand.TestSelectTextKeyboard, []))[1] ?? "");
      if (/visual=1/.test(dump)) {
        break;
      }
      await sleep(25);
    }
    if (!/visual=1/.test(dump)) {
      throw new Error(`selection-toolbar-stays: visual mode did not start\n${dump}`);
    }

    pressVKey(frame, VK_END);
    await waitForSelection(client, LINE);
    await waitForToolbarVisible(client);

    await clickToolbar(client, "CmdCopySelection");
    const afterCopy = await toolbarDump(client);
    if (!toolbarVisible(afterCopy)) {
      throw new Error(`selection-toolbar-stays: Copy hid the selection toolbar\n${afterCopy}`);
    }
    if ((await selectedText(client)) !== LINE) {
      throw new Error(`selection-toolbar-stays: Copy cleared the selection`);
    }

    await clickToolbar(client, "CmdCreateAnnotUnderline");
    await waitForUnderline(client);
    await waitForSelectionGone(client);
    const afterUnderline = await toolbarDump(client);
    if (toolbarVisible(afterUnderline)) {
      throw new Error(`selection-toolbar-stays: Underline left the selection toolbar up\n${afterUnderline}`);
    }
    const markup = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    if (!/state selected=1/.test(markup)) {
      throw new Error(`selection-toolbar-stays: the new underline was not selected\n${markup}`);
    }

    sendCommandSync(frame, cmdId("CmdDiscardChanges"));
    await client.waitForRenderIdle();
    console.log("selection-toolbar-stays: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
