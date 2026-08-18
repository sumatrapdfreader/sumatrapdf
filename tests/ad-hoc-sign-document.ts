// Ad-hoc test for the Sign Document dialog (issue #5962).
//
// Signs a PDF with a throw-away self-signed certificate and checks the result
// with the bundled pdfsign tool. Ad-hoc (not in run-almost-all) because it
// creates a certificate in the user's certificate store, drives a modal
// "Save As" dialog and depends on PowerShell's PKI cmdlets.
//
// Run:  bun tests/ad-hoc-sign-document.ts [--no-build]

import { copyFileSync, existsSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { EXE, cmdId, runStandalone, tmpPath } from "./util.ts";
import { clickAt, findCanvas, launchSumatra, pressEnter, sendCommand, waitForFrame } from "./win-automation.ts";
import {
  captureWindowToPng,
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getControlText,
  getWindowPid,
  getWindowText,
  postMessage,
  sendText,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

const kPassword = "sumatra-test-pw";
const kCertSubject = "CN=SumatraPDF SignTest";
const kToolExe = join(dirname(EXE), "sumatrapdf-tool.exe");

function ps(script: string): { ok: boolean; out: string } {
  const r = Bun.spawnSync(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script]);
  return { ok: r.exitCode === 0, out: r.stdout.toString() + r.stderr.toString() };
}

// Creates a signing certificate and exports it as a .pfx. Returns null (with a
// printed reason) when the machine won't let us, so the test can skip.
function makeTestCert(pfxPath: string): string | null {
  rmSync(pfxPath, { force: true });
  const created = ps(
    `(New-SelfSignedCertificate -Subject '${kCertSubject}' -CertStoreLocation Cert:\\CurrentUser\\My ` +
      `-KeyUsage DigitalSignature -Type Custom -KeySpec Signature).Thumbprint`,
  );
  const thumb = created.out.trim().split(/\s+/).pop() ?? "";
  if (!created.ok || !/^[0-9A-F]{40}$/i.test(thumb)) {
    console.log(`\nSKIP sign-document: could not create a test certificate:\n${created.out}`);
    return null;
  }
  const exported = ps(
    `$pw = ConvertTo-SecureString -String '${kPassword}' -Force -AsPlainText; ` +
      `Export-PfxCertificate -Cert Cert:\\CurrentUser\\My\\${thumb} -FilePath '${pfxPath}' -Password $pw | Out-Null`,
  );
  if (!exported.ok || !existsSync(pfxPath)) {
    console.log(`\nSKIP sign-document: could not export the test certificate:\n${exported.out}`);
    removeTestCert(thumb);
    return null;
  }
  return thumb;
}

function removeTestCert(thumbprint: string): void {
  ps(`Remove-Item -Path Cert:\\CurrentUser\\My\\${thumbprint} -Force -ErrorAction SilentlyContinue`);
}

// A one-page PDF whose only interesting feature is an empty signature field.
function writePdfWithEmptySigField(path: string): void {
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [6 0 R] /SigFlags 3 >> >>";
  objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
  objs[3] =
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 5 0 R >> >> " +
    "/Contents 4 0 R /Annots [6 0 R] >>";
  const content = "BT /F1 14 Tf 30 150 Td (please sign below) Tj ET";
  objs[4] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  objs[5] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";
  objs[6] =
    "<< /Type /Annot /Subtype /Widget /FT /Sig /T (CEO) /Rect [30 40 220 100] /F 4 /P 3 0 R /DA (/F1 0 Tf 0 g) >>";

  let pdf = "%PDF-1.7\n";
  const offsets: number[] = [];
  for (let i = 1; i < objs.length; i++) {
    offsets[i] = pdf.length;
    pdf += `${i} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefAt = pdf.length;
  pdf += `xref\n0 ${objs.length}\n0000000000 65535 f \n`;
  for (let i = 1; i < objs.length; i++) {
    pdf += String(offsets[i]).padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${objs.length} /Root 1 0 R >>\nstartxref\n${xrefAt}\n%%EOF\n`;
  writeFileSync(path, pdf, "latin1");
}

function findTopWindow(pid: number, className: string, title?: string): number {
  let found = 0;
  enumWindows((h) => {
    if (getWindowPid(h) !== pid || getClassName(h) !== className) {
      return true;
    }
    if (title !== undefined && getWindowText(h) !== title) {
      return true;
    }
    found = h;
    return false;
  });
  return found;
}

async function waitFor<T>(what: string, fn: () => T, timeoutMs = 12000): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const v = fn();
    if (v) {
      return v;
    }
    if (Date.now() > deadline) {
      throw new Error(`timed out waiting for ${what}`);
    }
    await sleep(200);
  }
}

function childrenOfClass(parent: number, className: string): number[] {
  const res: number[] = [];
  enumChildWindows(parent, (h) => {
    if (getClassName(h) === className) {
      res.push(h);
    }
    return true;
  });
  return res;
}

// Opens the dialog, fills it in and presses Sign (Enter activates the default
// button). Returns the dialog hwnd and what the placement drop-down offered.
async function signWith(pid: number, frame: number, pfx: string, password: string) {
  sendCommand(frame, cmdId("CmdSignDocument"));
  const dlg = await waitFor("Sign Document dialog", () =>
    findTopWindow(pid, "SumatraWgDefaultWinClass", "Sign Document"),
  );
  await sleep(600);
  const edits = childrenOfClass(dlg, "Edit");
  if (edits.length < 4) {
    throw new Error(`Sign dialog: expected 4 edit fields, got ${edits.length}`);
  }
  const combos = childrenOfClass(dlg, "ComboBox");
  const placement =
    combos.map(getControlText).find((t) => /signature|CEO|page/i.test(t)) ??
    (combos.length ? getControlText(combos[combos.length - 1]) : "");
  sendText(edits[0], pfx);
  sendText(edits[1], password);
  await sleep(300);
  await pressEnter(dlg);
  await sleep(2500);
  return { dlg, placement };
}

// Accepts the "Save As" dialog with the name it suggests.
async function acceptSaveDialog(pid: number): Promise<void> {
  const save = await waitFor("Save As dialog", () => findTopWindow(pid, "#32770", "Save As"));
  const edits = childrenOfClass(save, "Edit");
  await pressEnter(edits.length ? edits[0] : save);
}

function verifySignature(path: string): string {
  const r = Bun.spawnSync([kToolExe, "sign", "-v", path]);
  return (r.stdout.toString() + r.stderr.toString()).trim();
}

export async function testit(): Promise<void> {
  const pfx = tmpPath("sign-document.pfx");
  const thumbprint = makeTestCert(pfx);
  if (!thumbprint) {
    return; // machine can't make certificates: skip, don't fail
  }
  try {
    await runSigningChecks(pfx);
  } finally {
    removeTestCert(thumbprint);
    rmSync(pfx, { force: true });
  }
}

async function runSigningChecks(pfx: string): Promise<void> {
  // 1. a plain PDF: the dialog offers a new signature on the current page
  const plainIn = tmpPath("sign-document-plain.pdf");
  const plainOut = tmpPath("sign-document-plain Copy.pdf");
  copyFileSync(join(dirname(dirname(EXE)), "..", "ext", "a-zlib", "zlib.3.pdf"), plainIn);
  rmSync(plainOut, { force: true });

  let proc = launchSumatra([plainIn]);
  let pid = proc.pid!;
  let frame = await waitForFrame(pid);
  await sleep(2500);

  // a wrong password must not sign anything
  const bad = await signWith(pid, frame, pfx, "definitely-not-the-password");
  const msgBox = findTopWindow(pid, "#32770");
  if (!msgBox) {
    throw new Error("wrong password: expected an error message box");
  }
  captureWindowToPng(msgBox, tmpPath("sign-document-badpwd.png"));
  postMessage(msgBox, WM_CLOSE, 0, 0);
  await sleep(800);
  if (existsSync(plainOut)) {
    throw new Error("wrong password: must not have written a file");
  }
  if (!findTopWindow(pid, "SumatraWgDefaultWinClass", "Sign Document")) {
    throw new Error("wrong password: the dialog should stay open so the password can be fixed");
  }
  postMessage(bad.dlg, WM_CLOSE, 0, 0);
  await sleep(800);

  const good = await signWith(pid, frame, pfx, kPassword);
  if (!good.placement.includes("New signature")) {
    throw new Error(`expected a "new signature" placement, got '${good.placement}'`);
  }
  // a new signature is placed by clicking the page (issue #5967)
  const canvas = findCanvas(frame);
  if (!canvas) {
    throw new Error("plain PDF: could not find the canvas to place the signature");
  }
  const cr = getClientRect(canvas);
  await clickAt(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2), 400);
  await acceptSaveDialog(pid);
  await waitFor("signed file", () => existsSync(plainOut), 20000);
  await sleep(2000);
  postMessage(frame, WM_CLOSE, 0, 0);
  await sleep(1200);
  try {
    proc.kill();
  } catch {}

  let out = verifySignature(plainOut);
  if (!out.includes("Distinguished name") || !out.includes("The document is unchanged since signing")) {
    throw new Error(`signed file did not verify:\n${out}`);
  }

  // 2. a PDF that already has an empty signature field: that field gets filled
  // in rather than a second one added
  const fieldIn = tmpPath("sign-document-field.pdf");
  const fieldOut = tmpPath("sign-document-field Copy.pdf");
  writePdfWithEmptySigField(fieldIn);
  rmSync(fieldOut, { force: true });

  proc = launchSumatra([fieldIn]);
  pid = proc.pid!;
  frame = await waitForFrame(pid);
  await sleep(2500);

  const field = await signWith(pid, frame, pfx, kPassword);
  if (!field.placement.includes("CEO")) {
    throw new Error(`expected the document's empty 'CEO' field to be offered, got '${field.placement}'`);
  }
  await acceptSaveDialog(pid);
  await waitFor("signed file", () => existsSync(fieldOut), 20000);
  await sleep(2000);
  captureWindowToPng(frame, tmpPath("sign-document-field-after.png"));
  postMessage(frame, WM_CLOSE, 0, 0);
  await sleep(1200);
  try {
    proc.kill();
  } catch {}

  out = verifySignature(fieldOut);
  if (!out.includes("Distinguished name") || !out.includes("The document is unchanged since signing")) {
    throw new Error(`signed file with an existing field did not verify:\n${out}`);
  }
  // the existing field is object 6 in the PDF written above; signing it must
  // not have created another signature
  if (!out.includes("Verifying signature 6:") || out.split("Verifying signature").length !== 2) {
    throw new Error(`expected exactly the existing signature field to be signed:\n${out}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
