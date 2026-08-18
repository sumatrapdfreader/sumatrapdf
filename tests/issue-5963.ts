// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5963
//
// Sign Document used to always draw labels ("Digitally signed by"), the
// distinguished name, the date, and a large name graphic. The dialog now has
// appearance checkboxes and an optional PNG/JPEG. This checks the dialog
// offers those controls, and (when the machine can make a test cert) that
// signing with an image and without labels embeds an image XObject.
//
// Run:  bun tests/issue-5963.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, rmSync, writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getControlText,
  getWindowPid,
  getWindowText,
  postMessage,
  sendMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

const kCertSubject = "CN=SumatraPDF AppearSignTest";
const BM_GETCHECK = 0x00f0;
const BST_CHECKED = 1;
// dn | date | text name — no labels, no graphic name
const kNoLabelsFlags = 2 | 4 | 8;

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
    console.log(`\nSKIP issue-5963 sign: could not create a test certificate:\n${created.out}`);
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
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Kids [3 0 R] /Count 1 >>";
  objs[3] =
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 5 0 R >> >> " +
    "/Contents 4 0 R >>";
  const content = "BT /F1 12 Tf 20 160 Td (sign here) Tj ET";
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

// 1x1 red PNG so the signed file has an obvious image XObject
function writeTinyPng(path: string): void {
  const b64 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==";
  writeFileSync(path, Buffer.from(b64, "base64"));
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

async function waitFor<T>(what: string, fn: () => T, timeoutMs = 8000): Promise<T> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const v = fn();
    if (v) {
      return v;
    }
    if (Date.now() > deadline) {
      throw new Error(`timed out waiting for ${what}`);
    }
    await sleep(100);
  }
}

function stripAmp(s: string): string {
  return s.replace(/&/g, "");
}

function checkboxTexts(dlg: number): Map<string, number> {
  const map = new Map<string, number>();
  enumChildWindows(dlg, (h) => {
    if (getClassName(h) === "Button") {
      const t = stripAmp(getControlText(h));
      if (t) {
        map.set(t, h);
      }
    }
    return true;
  });
  return map;
}

function isChecked(hwnd: number): boolean {
  return Number(sendMessage(hwnd, BM_GETCHECK, 0, 0)) === BST_CHECKED;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5963.pdf");
  writePlainPdf(pdf);

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const pid = getWindowPid(frame);
    sendCommand(frame, cmdId("CmdSignDocument"));
    const dlg = await waitFor("Sign Document dialog", () =>
      findTopWindow(pid, "SumatraWgDefaultWinClass", "Sign Document"),
    );
    await sleep(400);

    let boxes = new Map<string, number>();
    for (const deadline = Date.now() + 3000; Date.now() < deadline;) {
      boxes = checkboxTexts(dlg);
      if ([...boxes.keys()].some((t) => /labels/i.test(t))) {
        break;
      }
      await sleep(100);
    }
    const want = [
      [/labels/i, "Show labels"],
      [/^show name$/i, "Show name"],
      [/dn/i, "Show DN"],
      [/date/i, "Show date"],
      [/graphic/i, "Show name as graphic"],
    ] as const;
    for (const [re, label] of want) {
      const hwnd = [...boxes.entries()].find(([t]) => re.test(t))?.[1];
      if (!hwnd) {
        throw new Error(`issue-5963: missing '${label}' checkbox; have: ${[...boxes.keys()].join(", ")}`);
      }
      if (!isChecked(hwnd)) {
        throw new Error(`issue-5963: '${label}' should be checked by default`);
      }
    }
    console.log("  Sign Document offers appearance checkboxes, all on by default ✓");
    postMessage(dlg, WM_CLOSE, 0, 0);
    await sleep(200);

    const thumb = makeTestCert();
    if (!thumb) {
      return;
    }

    const png = tmpPath("issue-5963.png");
    const signed = tmpPath("issue-5963-signed.pdf");
    writeTinyPng(png);
    rmSync(signed, { force: true });
    try {
      const signedRes = await client.request(ControlCommand.TestSignDocument, [
        pdf,
        signed,
        thumb,
        "",
        "",
        png,
        kNoLabelsFlags,
      ]);
      const signRaw = String(signedRes[1] ?? "");
      if (signedRes[0] !== 0) {
        throw new Error(`issue-5963 sign: ${signRaw.trim()}`);
      }
      if (!existsSync(signed)) {
        throw new Error("issue-5963: signed file was not written");
      }
      const bytes = new TextDecoder("latin1").decode(await Bun.file(signed).arrayBuffer());
      if (!/\/Subtype\s*\/Image/.test(bytes)) {
        throw new Error("issue-5963: signed file has no image XObject");
      }
      console.log("  signed with an image and without labels; PDF embeds the image ✓");
    } finally {
      removeTestCert(thumb);
      rmSync(signed, { force: true });
      rmSync(png, { force: true });
    }
  } finally {
    client.close();
    postMessage(frame, WM_CLOSE, 0, 0);
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
