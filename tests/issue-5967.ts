// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5967
//
// Adding a new signature ("New signature on page N") used to dump it in
// mupdf's default corner. Sign now hides the dialog and waits for a click or
// drag on the page. This test: Sign without a selection must not open Save As
// yet; after a click on the page, Save As appears and the written signature
// is not at the default {12,12,112,62} box.
//
// Needs a Windows-store signing certificate. Creates a throw-away one; skips
// (does not fail) if this machine cannot create one and has none already.
// Not in run-almost-all / run-all: after placing the signature it opens the
// system's Save As dialog, which needs someone at the keyboard.
//
// Run:  bun tests/issue-5967.ts [--no-build]

import { existsSync, rmSync, writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled, pressEnter, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getClientRect,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  packCoords,
  postMessage,
  sleep,
  WM_CLOSE,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  MK_LBUTTON,
} from "./winapi.ts";

const kCertSubject = "CN=SumatraPDF PlaceSignTest";
const kDefaultRect = /\[\s*12(\.0+)?\s+12(\.0+)?\s+112(\.0+)?\s+62(\.0+)?\s*\]/;

function ps(script: string): { ok: boolean; out: string } {
  const r = Bun.spawnSync(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script]);
  return { ok: r.exitCode === 0, out: r.stdout.toString() + r.stderr.toString() };
}

function makeTestCert(): string | null {
  const created = ps(`
    $rsa = [System.Security.Cryptography.RSA]::Create(2048)
    $req = [System.Security.Cryptography.X509Certificates.CertificateRequest]::new(
      '${kCertSubject}', $rsa,
      [System.Security.Cryptography.HashAlgorithmName]::SHA256,
      [System.Security.Cryptography.RSASignaturePadding]::Pkcs1)
    $cert = $req.CreateSelfSigned([datetime]::UtcNow.AddDays(-1), [datetime]::UtcNow.AddYears(1))
    $flags = [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::PersistKeySet -bor
             [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable
    $stored = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
      $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Pfx, 'x'), 'x', $flags)
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store('My','CurrentUser')
    $store.Open('ReadWrite')
    $store.Add($stored)
    $store.Close()
    Write-Output $stored.Thumbprint
  `);
  const thumb = created.out.trim().split(/\s+/).pop() ?? "";
  if (!created.ok || !/^[0-9A-F]{40}$/i.test(thumb)) {
    console.log(`\nSKIP issue-5967: could not create a test certificate:\n${created.out}`);
    return null;
  }
  return thumb.toUpperCase();
}

function removeTestCert(thumbprint: string): void {
  ps(`
    $store = New-Object System.Security.Cryptography.X509Certificates.X509Store('My','CurrentUser')
    $store.Open('ReadWrite')
    foreach ($c in $store.Certificates.Find('FindByThumbprint', '${thumbprint}', $false)) {
      $store.Remove($c)
    }
    $store.Close()
  `);
}

function writePlainPdf(path: string): void {
  // letter-size so the page fills the test window and a click near the
  // canvas origin lands on the page, not the background
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
  objs[3] =
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> " +
    "/Contents 4 0 R >>";
  const content = "BT /F1 14 Tf 72 720 Td (sign anywhere) Tj ET";
  objs[4] = `<< /Length ${content.length} >>\nstream\n${content}\nendstream`;
  objs[5] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";

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
    await sleep(150);
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

async function acceptSaveDialog(pid: number): Promise<void> {
  const save = await waitFor("Save As dialog", () => findTopWindow(pid, "#32770", "Save As"));
  const edits = childrenOfClass(save, "Edit");
  await pressEnter(edits.length ? edits[0] : save);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5967.pdf");
  const signed = tmpPath("issue-5967 Copy.pdf");
  writePlainPdf(pdf);
  rmSync(signed, { force: true });

  const { proc, client, frame } = await launchControlled([pdf]);
  let createdThumb: string | null = null;
  try {
    await client.waitForRenderIdle();

    let listed = await client.request(ControlCommand.TestListSigningCerts);
    let listRaw = String(listed[1] ?? "");
    if (listed[0] !== 0 || !/n=([1-9]\d*)/.test(listRaw)) {
      createdThumb = makeTestCert();
      if (!createdThumb) {
        return;
      }
      listed = await client.request(ControlCommand.TestListSigningCerts);
      listRaw = String(listed[1] ?? "");
      if (listed[0] !== 0 || !listRaw.toUpperCase().includes(`THUMB=${createdThumb}`)) {
        console.log(`\nSKIP issue-5967: store still has no signing cert:\n${listRaw}`);
        return;
      }
    }

    const pid = getWindowPid(frame);
    sendCommand(frame, cmdId("CmdSignDocument"));
    const dlg = await waitFor("Sign Document dialog", () =>
      findTopWindow(pid, "SumatraWgDefaultWinClass", "Sign Document"),
    );
    await sleep(400);
    await pressEnter(dlg);

    // without the fix, Sign writes the default-corner box and opens Save As
    await sleep(800);
    if (findTopWindow(pid, "#32770", "Save As")) {
      throw new Error("issue-5967: Sign opened Save As before the signature was placed");
    }
    if (isWindowVisible(dlg)) {
      throw new Error("issue-5967: Sign dialog stayed visible; expected it to hide for placement");
    }
    console.log("  Sign hid the dialog and did not save yet ✓");

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-5967: could not find the canvas");
    }
    const cr = getClientRect(canvas);
    // near the top-left of the canvas: the page is drawn from there at
    // typical test-window zoom, so this is on the page
    const x = Math.min(80, Math.max(20, Math.floor(cr.right / 8)));
    const y = Math.min(80, Math.max(20, Math.floor(cr.bottom / 8)));
    postMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
    postMessage(canvas, WM_LBUTTONUP, 0, packCoords(x, y));
    await sleep(600);

    const anyDlg = findTopWindow(pid, "#32770");
    if (anyDlg && getWindowText(anyDlg) !== "Save As") {
      throw new Error(`issue-5967: unexpected dialog '${getWindowText(anyDlg)}' after placing`);
    }
    if (isWindowVisible(dlg)) {
      throw new Error("issue-5967: Sign dialog came back after the click; signing the placed box failed");
    }

    await acceptSaveDialog(pid);
    await waitFor("signed file", () => existsSync(signed), 20000);
    await sleep(800);

    const bytes = new TextDecoder("latin1").decode(await Bun.file(signed).arrayBuffer());
    const rects = [...bytes.matchAll(/\/Rect\s*\[([^\]]+)\]/g)].map((m) => m[1].trim());
    if (rects.length === 0) {
      throw new Error("issue-5967: signed file has no /Rect");
    }
    if (kDefaultRect.test(bytes)) {
      throw new Error(
        `issue-5967: signature used mupdf's default [12 12 112 62] corner box; rects: ${rects.join("; ")}`,
      );
    }
    if (!/\/FT\s*\/Sig/.test(bytes) && !/\/Type\s*\/Sig/.test(bytes)) {
      throw new Error("issue-5967: signed file has no signature field");
    }
    console.log(`  click placed the signature at [${rects[rects.length - 1]}] ✓`);
  } finally {
    client.close();
    postMessage(frame, WM_CLOSE, 0, 0);
    await killAndWait(proc);
    if (createdThumb) {
      removeTestCert(createdThumb);
    }
    try {
      rmSync(signed, { force: true });
    } catch {
      // the signed file can stay locked briefly after kill
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
