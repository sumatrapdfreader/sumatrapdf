// Compress, Decompress, Encrypt, Decrypt and Delete Pages re-open the PDF from
// disk, so they dropped annotations that weren't saved yet. Their output must
// keep an unsaved stamp.
//
// Run: bun tests/pdf-tools-unsaved-annots.ts [--no-build] [-ng]

import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { EXE, USE_NG, cmdId, makeTopBottomPdf, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, pressKey, sendCommand, waitToolWindow } from "./win-automation.ts";
import { enumChildWindows, getClassName, getControlText, sendText, VK_RETURN } from "./winapi.ts";

const name = "pdf-tools-unsaved-annots";
const kPassword = "secret";

type Tool = { cmd: string; title: string; ngName: string; action?: string; pages?: string; password?: boolean };

const tools: Tool[] = [
  { cmd: "CmdPdfCompress", title: "Compress PDF", ngName: "pdfcompress" },
  { cmd: "CmdPdfDecompress", title: "Decompress PDF", ngName: "pdfdecompress" },
  {
    cmd: "CmdPdfDeletePages",
    title: "Delete Pages From PDF",
    ngName: "pdfdeletepages",
    action: "Delete Pages",
    pages: "2",
  },
  { cmd: "CmdPdfEncrypt", title: "Encrypt PDF", ngName: "pdfencrypt", password: true },
];
const decrypt: Tool = { cmd: "CmdPdfDecrypt", title: "Decrypt PDF", ngName: "pdfdecrypt" };

// orig: fill the dialog's Win32 edits and press Enter in the destination edit
async function runOrigDialog(pid: number, client: ControlClient, tool: Tool): Promise<void> {
  const dlg = await waitToolWindow(client, pid, "pdftool", { title: tool.title });
  if (!dlg) {
    throw new Error(`${name}: no dialog '${tool.title}'`);
  }
  const edits: number[] = [];
  enumChildWindows(dlg, (hwnd) => {
    if (getClassName(hwnd) === "Edit") {
      edits.push(hwnd);
    }
    return true;
  });
  const destEdit = edits.find((h) => /\.pdf$/i.test(getControlText(h)));
  if (!destEdit) {
    throw new Error(`${name}: '${tool.title}' has no destination edit`);
  }
  const other = edits.find((h) => h !== destEdit);
  const text = tool.password ? kPassword : tool.pages;
  if (text && other) {
    sendText(other, text);
  }
  await pressKey(destEdit, VK_RETURN, 0);
}

async function ngLayout(client: ControlClient, tool: Tool): Promise<string[]> {
  const res = await client.request(ControlCommand.TestToolWindow, ["layout", tool.ngName, "", 0, 0, 0, 0]);
  return String(res[1] ?? "").split("\n");
}

function center(line: string): { x: number; y: number } {
  const m = /rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(line)!;
  return { x: +m[1]! + Math.floor(+m[3]! / 2), y: +m[2]! + Math.floor(+m[4]! / 2) };
}

async function ngClick(client: ControlClient, tool: Tool, at: { x: number; y: number }): Promise<void> {
  await client.request(ControlCommand.TestToolWindow, ["input", tool.ngName, "click", at.x, at.y, 0, 0]);
}

// ng: type into the focused input (the dialog focuses its first one), then click the action button
async function runNgDialog(client: ControlClient, tool: Tool): Promise<void> {
  const button = await pollUntil(
    async () =>
      (await ngLayout(client, tool)).find(
        (l) => l.startsWith("node ") && l.includes(`label='${tool.action ?? tool.title}'`),
      ),
    (l) => !!l,
    { timeoutMs: 10000 * SLOW_BUILD_FACTOR, error: () => `${name}: no '${tool.action ?? tool.title}' button` },
  );
  const text = tool.password ? kPassword : tool.pages;
  if (text) {
    if (tool.pages) {
      // select the default range so typing replaces it
      const kCtrl = 2;
      const kVkA = 0x41;
      await client.request(ControlCommand.TestToolWindow, ["input", tool.ngName, "key", kVkA, kCtrl, 0, 0]);
    }
    for (const ch of text) {
      await client.request(ControlCommand.TestToolWindow, ["input", tool.ngName, "char", ch.charCodeAt(0), 0, 0, 0]);
    }
  }
  await ngClick(client, tool, center(button!));
}

function hasStamp(pdf: string, password: string): boolean {
  const plain = `${pdf}.plain.pdf`;
  const args = password ? ["clean", "-p", password, "-d", pdf, plain] : ["clean", "-d", pdf, plain];
  const p = Bun.spawnSync([EXE, ...args]);
  if (p.exitCode !== 0) {
    throw new Error(`${name}: decompressing ${pdf} failed: ${p.stderr.toString()}`);
  }
  return /\/Subtype\s*\/Stamp/.test(readFileSync(plain, "latin1"));
}

// opens src, adds a stamp without saving, runs the tool and checks its output keeps it
async function runTool(dir: string, src: string, launchArgs: string[], tool: Tool, outPassword: string): Promise<void> {
  const before = new Set(readdirSync(dir));
  const { proc, client, frame } = await launchControlled([...launchArgs, src]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const stamp = await client.request(ControlCommand.TestInvokeCommand, ["CmdCreateAnnotStamp", 80, 80]);
    if (stamp[0] !== 0) {
      throw new Error(`${name}: create stamp: ${String(stamp[1] ?? "")}`);
    }
    sendCommand(frame, cmdId(tool.cmd));
    if (USE_NG) {
      await runNgDialog(client, tool);
    } else {
      await runOrigDialog(proc.pid!, client, tool);
    }
    const out = await pollUntil(
      () => readdirSync(dir).find((f) => !before.has(f) && f.endsWith(".pdf")),
      (f) => !!f && existsSync(join(dir, f)),
      { timeoutMs: 30000 * SLOW_BUILD_FACTOR, error: () => `${name}: ${tool.title}: no output in ${dir}` },
    );
    // written once the app opens it; an encrypted output waits in its password dialog
    if (outPassword) {
      await pollUntil(
        () => readFileSync(join(dir, out!)).includes("%%EOF"),
        (ok) => ok,
      );
    } else {
      await client.waitForRenderIdle();
    }
    if (!hasStamp(join(dir, out!), outPassword)) {
      throw new Error(`${name}: ${tool.title} dropped the unsaved annotation`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  for (const tool of tools) {
    const sub = join(dir, tool.ngName);
    mkdirSync(sub);
    const src = join(sub, "src.pdf");
    writeFileSync(src, makeTopBottomPdf(2));
    await runTool(sub, src, [], tool, tool.password ? kPassword : "");
    console.log(`${name}: ${tool.title} OK`);
  }

  const sub = join(dir, decrypt.ngName);
  mkdirSync(sub);
  const plain = join(dir, "plain.pdf");
  const src = join(sub, "src.pdf");
  writeFileSync(plain, makeTopBottomPdf(2));
  const p = Bun.spawnSync([EXE, "clean", "-E", "aes-256", "-U", kPassword, "-O", kPassword, plain, src]);
  if (p.exitCode !== 0) {
    throw new Error(`${name}: encrypting failed: ${p.stderr.toString()}`);
  }
  await runTool(sub, src, ["-pwd", kPassword], decrypt, "");
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
