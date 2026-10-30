// Decrypt PDF on a document unlocked with the key remembered in file history:
// the engine has the key but not the password, and the command did nothing.
// When the decrypted copy can't be written, the failure must be reported.

import { existsSync, mkdirSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, USE_NG, cmdId, extractPageText, makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, pressEnter, sendCommand, waitToolWindow } from "./win-automation.ts";
import { findChildWindow, getControlText } from "./winapi.ts";

export const kPassword = "pw";
const kText = "decrypted line";
const kDecryptTitle = "Decrypt PDF";
const kKeyLen = 32;
const kRc4Bytes = 5;
const kPdfPad = "28bf4e5e4e758a4164004e56fffa01082e2e00b6d0683e802f0ca9fe6453697a";

function md5(...parts: Uint8Array[]): Buffer {
  const h = new Bun.CryptoHasher("md5");
  for (const p of parts) {
    h.update(p);
  }
  return Buffer.from(h.digest());
}

// the PDF string after re: mupdf writes one in hex or as a literal, depending on its bytes
function stringField(pdf: string, re: RegExp): Buffer {
  const m = re.exec(pdf);
  if (!m) {
    throw new Error(`pdf-decrypt-remembered-key: no ${re} in the encrypted file`);
  }
  let i = m.index + m[0].length;
  if (pdf[i] === "<") {
    return Buffer.from(pdf.slice(i + 1, pdf.indexOf(">", i)), "hex");
  }
  const kEscapes: Record<string, string> = { n: "\n", r: "\r", t: "\t", b: "\b", f: "\f" };
  const out: number[] = [];
  let depth = 1;
  for (i++; depth > 0; i++) {
    let c = pdf[i]!;
    if (c === "\\") {
      const next = pdf[++i]!;
      const octal = /^[0-7]{1,3}/.exec(pdf.slice(i, i + 3));
      if (octal) {
        out.push(parseInt(octal[0], 8) & 0xff);
        i += octal[0].length - 1;
        continue;
      }
      c = kEscapes[next] ?? next;
    } else if (c === "(") {
      depth++;
    } else if (c === ")" && --depth === 0) {
      break;
    }
    out.push(c.charCodeAt(0));
  }
  return Buffer.from(out);
}

// what file history stores: md5 of the file, then the key (PDF spec algorithm 2, RC4 40-bit)
export function rememberedKey(path: string): string {
  const bytes = readFileSync(path);
  const pdf = bytes.toString("latin1");
  const o = stringField(pdf, /\/O\s*(?=[<(])/);
  const id = stringField(pdf, /\/ID\s*\[\s*/);
  const pm = /\/P\s+(-?\d+)/.exec(pdf);
  if (!pm) {
    throw new Error("pdf-decrypt-remembered-key: no /P in the encrypted file");
  }
  const p = Buffer.alloc(4);
  p.writeInt32LE(Number(pm[1]));
  const pad = Buffer.concat([Buffer.from(kPassword), Buffer.from(kPdfPad, "hex")]).subarray(0, kKeyLen);
  const key = Buffer.alloc(kKeyLen);
  md5(pad, o, p, id).copy(key, 0, 0, kRc4Bytes);
  return md5(bytes).toString("hex") + key.toString("hex");
}

function makeEncryptedPdf(dir: string, method: string): string {
  const plain = join(dir, "plain.pdf");
  const encrypted = join(dir, `${method}.pdf`);
  writeFileSync(plain, makeTextPdf(kText));
  const p = Bun.spawnSync([EXE, "clean", "-E", method, "-U", kPassword, "-O", "owner", plain, encrypted]);
  if (p.exitCode !== 0) {
    throw new Error(`pdf-decrypt-remembered-key: encrypting failed: ${p.stderr.toString()}`);
  }
  return encrypted;
}

// opens the Decrypt dialog and returns its edit with the destination path
async function openDecrypt(app: Awaited<ReturnType<typeof launchControlled>>): Promise<number> {
  await app.client.waitForRenderIdle();
  sendCommand(app.frame, cmdId("CmdPdfDecrypt"));
  const dlg = await waitToolWindow(app.client, app.proc.pid!, "pdftool", { title: kDecryptTitle });
  if (!dlg) {
    throw new Error("pdf-decrypt-remembered-key: no dialog for a document unlocked with a remembered key");
  }
  return findChildWindow(dlg, "Edit");
}

async function quit(app: Awaited<ReturnType<typeof launchControlled>>): Promise<void> {
  await app.client.quit();
  await app.proc.exited;
}

async function decryptAndCheck(args: string[]): Promise<void> {
  const app = await launchControlled(args);
  try {
    const edit = await openDecrypt(app);
    const dest = getControlText(edit);
    await pressEnter(edit);
    await pollUntil(
      () => existsSync(dest),
      (ok) => ok,
    );
    if (readFileSync(dest).includes("/Encrypt")) {
      throw new Error("pdf-decrypt-remembered-key: the copy is still encrypted");
    }
    const text = extractPageText(dest);
    if (!text.includes(kText)) {
      throw new Error(`pdf-decrypt-remembered-key: the copy has the wrong text: '${text}'`);
    }
  } finally {
    await quit(app);
  }
}

export async function testit(): Promise<void> {
  // ng's tool dialogs have no Win32 edit to drive
  if (USE_NG) {
    console.log("skip pdf-decrypt-remembered-key: orig only");
    return;
  }
  const dir = tmpPath("pdf-decrypt-remembered-key");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata);
  const log = join(dir, "log.txt");
  const encrypted = makeEncryptedPdf(dir, "rc4-40");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    `FileStates [\n\t[\n\t\tFilePath = ${encrypted}\n\t\tDecryptionKey = ${rememberedKey(encrypted)}\n\t]\n]\n`,
  );

  const first = await launchControlled(["-appdata", appdata, "-log-to-file", log, encrypted]);
  try {
    const edit = await openDecrypt(first);
    const dest = getControlText(edit);

    // the file changed since it was opened, so the key may no longer fit it
    const later = new Date(Date.now() + 60_000);
    utimesSync(encrypted, later, later);
    await pressEnter(edit);
    await pollUntil(
      () => (existsSync(log) ? readFileSync(log, "utf8") : ""),
      (s) => s.includes("PdfDecryptDoIt: failed"),
    );
    if (existsSync(dest)) {
      throw new Error("pdf-decrypt-remembered-key: wrote a copy of a file that changed");
    }
  } finally {
    await quit(first);
  }

  // reopened, so the file on disk is again the one the engine loaded
  await decryptAndCheck(["-appdata", appdata, encrypted]);

  // AES-256, opened with a password
  await decryptAndCheck(["-pwd", kPassword, makeEncryptedPdf(dir, "aes-256")]);
}

if (import.meta.main) {
  await runStandalone(testit);
}
