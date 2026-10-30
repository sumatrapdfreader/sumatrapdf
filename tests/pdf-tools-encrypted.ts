// The PDF tools re-open the file from disk, without the password or the key the
// document was unlocked with, so they all failed on a password-protected PDF.
// Their output must stay encrypted: only Decrypt PDF removes the password.

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { EXE, USE_NG, cmdId, extractPageText, makeTopBottomPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, pressEnter, sendCommand, waitToolWindow } from "./win-automation.ts";
import { enumChildWindows, getClassName, getControlText, sendText } from "./winapi.ts";
import { kPassword, rememberedKey } from "./pdf-decrypt-remembered-key.ts";

const kPages = 2;
const kPage1 = "page 1 top";
const kPage2 = "page 2 top";

type App = Awaited<ReturnType<typeof launchControlled>>;
type Launch = () => Promise<App>;

function encrypt(dir: string, method: string): string {
  const plain = join(dir, `${method}-plain.pdf`);
  const encrypted = join(dir, `${method}.pdf`);
  writeFileSync(plain, makeTopBottomPdf(kPages));
  const p = Bun.spawnSync([EXE, "clean", "-E", method, "-U", kPassword, "-O", "owner", plain, encrypted]);
  if (p.exitCode !== 0) {
    throw new Error(`pdf-tools-encrypted: encrypting failed: ${p.stderr.toString()}`);
  }
  return encrypted;
}

// the output needs the original password, and has the wanted pages
function checkEncryptedOutput(name: string, out: string, want: string[], notWant: string[] = []): void {
  if (!readFileSync(out).includes("/Encrypt")) {
    throw new Error(`pdf-tools-encrypted: ${name}: the output lost its encryption`);
  }
  const plain = `${out}.plain.pdf`;
  const p = Bun.spawnSync([EXE, "clean", "-p", kPassword, "-D", out, plain]);
  if (p.exitCode !== 0) {
    throw new Error(`pdf-tools-encrypted: ${name}: the password doesn't open the output: ${p.stderr.toString()}`);
  }
  const text = extractPageText(plain);
  for (const s of want) {
    if (!text.includes(s)) {
      throw new Error(`pdf-tools-encrypted: ${name}: no '${s}' in '${text}'`);
    }
  }
  for (const s of notWant) {
    if (text.includes(s)) {
      throw new Error(`pdf-tools-encrypted: ${name}: unexpected '${s}' in '${text}'`);
    }
  }
}

// not a quit: the encrypted output opens in a tab and its password dialog is modal
async function stop(app: App): Promise<void> {
  app.client.close();
  await killAndWait(app.proc);
}

// runs a tool dialog and returns the file it wrote; pages goes in the dialog's page range edit
async function runDialog(launch: Launch, cmd: string, title: string, pages = ""): Promise<string> {
  const app = await launch();
  try {
    await app.client.waitForRenderIdle();
    sendCommand(app.frame, cmdId(cmd));
    const dlg = await waitToolWindow(app.client, app.proc.pid!, "pdftool", { title });
    if (!dlg) {
      throw new Error(`pdf-tools-encrypted: no dialog '${title}'`);
    }
    const edits: number[] = [];
    enumChildWindows(dlg, (hwnd) => {
      if (getClassName(hwnd) === "Edit") {
        edits.push(hwnd);
      }
      return true;
    });
    const destEdit = edits.find((h) => /\.(pdf|txt)$/i.test(getControlText(h)));
    if (!destEdit) {
      throw new Error(`pdf-tools-encrypted: '${title}' has no destination edit`);
    }
    const dest = getControlText(destEdit);
    const pagesEdit = edits.find((h) => h !== destEdit);
    if (pages && pagesEdit) {
      sendText(pagesEdit, pages);
    }
    await pressEnter(destEdit);
    await pollUntil(
      () => existsSync(dest),
      (ok) => ok,
    );
    return dest;
  } finally {
    await stop(app);
  }
}

async function runExtractPages(launch: Launch, out: string): Promise<void> {
  const app = await launch();
  try {
    await app.client.waitForRenderIdle();
    const res = await app.client.request(ControlCommand.TestExtractPages, [out, "2", 0]);
    if (res[0] !== 0) {
      throw new Error(`pdf-tools-encrypted: extract pages: ${String(res[1] ?? "").trim()}`);
    }
  } finally {
    await stop(app);
  }
}

async function runMerge(launch: Launch, out: string): Promise<void> {
  const app = await launch();
  try {
    await app.client.waitForRenderIdle();
    await app.client.setNotificationsEnabled(false);
    sendCommand(app.frame, cmdId("CmdMergePDF"));
    await pollUntil(
      async () => (await app.client.request(ControlCommand.TestMergePdf, ["", "", 0]))[0],
      (code) => code === 0,
    );
    const res = await app.client.request(ControlCommand.TestMergePdf, ["saveas", out, 0]);
    if (String(res[1] ?? "") !== "OK saved=1") {
      throw new Error(`pdf-tools-encrypted: merge: ${String(res[1] ?? "")}`);
    }
  } finally {
    await stop(app);
  }
}

export async function testit(): Promise<void> {
  // ng's tool dialogs have no Win32 edit to drive
  if (USE_NG) {
    console.log("skip pdf-tools-encrypted: orig only");
    return;
  }
  const dir = tmpPath("pdf-tools-encrypted");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  // unlocked with a key remembered in file history: the engine has no password
  const appdata = join(dir, "appdata");
  mkdirSync(appdata);
  const rc4 = encrypt(dir, "rc4-40");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    `FileStates [\n\t[\n\t\tFilePath = ${rc4}\n\t\tDecryptionKey = ${rememberedKey(rc4)}\n\t]\n]\n`,
  );
  const withKey: Launch = () => launchControlled(["-appdata", appdata, rc4]);
  const both = [kPage1, kPage2];

  checkEncryptedOutput("compress", await runDialog(withKey, "CmdPdfCompress", "Compress PDF"), both);
  checkEncryptedOutput("decompress", await runDialog(withKey, "CmdPdfDecompress", "Decompress PDF"), both);
  checkEncryptedOutput("bake", await runDialog(withKey, "CmdPdfBake", "Bake PDF"), both);
  const deleted = await runDialog(withKey, "CmdPdfDeletePages", "Delete Pages From PDF", "1");
  checkEncryptedOutput("delete pages", deleted, [kPage2], [kPage1]);

  const extracted = join(dir, "extracted.pdf");
  await runExtractPages(withKey, extracted);
  checkEncryptedOutput("extract pages", extracted, [kPage2], [kPage1]);

  const merged = join(dir, "merged.pdf");
  await runMerge(withKey, merged);
  checkEncryptedOutput("merge", merged, both);

  // the text of a password-protected PDF is what the user asked for: not encrypted
  const txt = await runDialog(withKey, "CmdDocumentExtractText", "Extract Text From PDF");
  if (!readFileSync(txt, "utf8").includes(kPage1)) {
    throw new Error("pdf-tools-encrypted: extract text: wrong text");
  }

  // AES-256, opened with a password
  const aes = encrypt(dir, "aes-256");
  const withPwd: Launch = () => launchControlled(["-pwd", kPassword, aes]);
  checkEncryptedOutput("aes compress", await runDialog(withPwd, "CmdPdfCompress", "Compress PDF"), both);
}

if (import.meta.main) {
  await runStandalone(testit);
}
