// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5965
//
// Sign Document used to accept only a .pfx / .p12 file. It now lists
// certificates from the current user's Windows store and can sign with one
// without exporting it. This drives the engine path the dialog uses: create a
// throw-away self-signed cert, confirm ListWindowsSigningCertificates sees it,
// sign a PDF by thumbprint (no file), and check the result (via
// sumatrapdf-tool sign -v when that exe is present).
//
// Skips (does not fail) if this machine cannot create a test certificate.
//
// Run:  bun tests/issue-5965.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const kCertSubject = "CN=SumatraPDF StoreSignTest";

function findToolExe(): string | null {
  const candidates = [
    join(dirname(EXE), "sumatrapdf-tool.exe"),
    join(ROOT, "out", "dbg64", "sumatrapdf-tool.exe"),
    join(ROOT, "out", "rel64", "sumatrapdf-tool.exe"),
  ];
  return candidates.find((p) => existsSync(p)) ?? null;
}

function ps(script: string): { ok: boolean; out: string } {
  const r = Bun.spawnSync(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", script]);
  return { ok: r.exitCode === 0, out: r.stdout.toString() + r.stderr.toString() };
}

// Uses .NET CertificateRequest so it works even when the Cert: PSDrive is missing
// (New-SelfSignedCertificate needs that drive). PersistKeySet keeps the private
// key in the user store after this process exits.
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
    console.log(`\nSKIP issue-5965: could not create a test certificate:\n${created.out}`);
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

// CI's ASan job only builds SumatraPDF-static.exe, so the tool may be missing.
// Prefer `sign -v` when we have it; otherwise check the incremental signature
// dictionary the signer writes.
function verifySignedPdf(path: string): void {
  const tool = findToolExe();
  if (tool) {
    const r = Bun.spawnSync([tool, "sign", "-v", path]);
    const out = (r.stdout.toString() + r.stderr.toString()).trim();
    if (!out.includes("Distinguished name") || !out.includes("The document is unchanged since signing")) {
      throw new Error(`issue-5965: signed file did not verify:\n${out}`);
    }
    console.log("  verified with sumatrapdf-tool sign -v ✓");
    return;
  }
  const text = readFileSync(path).toString("latin1");
  if (!/\/Type\s*\/Sig/.test(text) || !/\/ByteRange/.test(text) || !/\/Contents/.test(text)) {
    throw new Error("issue-5965: signed file has no signature dictionary");
  }
  console.log("  signed PDF has a signature dictionary (no sumatrapdf-tool.exe) ✓");
}

export async function testit(): Promise<void> {
  const thumb = makeTestCert();
  if (!thumb) {
    return;
  }
  const pdf = tmpPath("issue-5965.pdf");
  const signed = tmpPath("issue-5965-signed.pdf");
  writePdfWithEmptySigField(pdf);
  rmSync(signed, { force: true });

  try {
    await withControlledSumatra(EXE, async (client) => {
      const listed = await client.request(ControlCommand.TestListSigningCerts);
      const listRaw = String(listed[1] ?? "");
      if (listed[0] !== 0) {
        throw new Error(`issue-5965 list: ${listRaw.trim()}`);
      }
      if (!listRaw.toUpperCase().includes(`THUMB=${thumb}`)) {
        throw new Error(`issue-5965: store list does not include the test cert:\n${listRaw}`);
      }
      console.log(`  store lists the test cert ${thumb.slice(0, 8)}… ✓`);

      const signedRes = await client.request(ControlCommand.TestSignDocument, [pdf, signed, thumb]);
      const signRaw = String(signedRes[1] ?? "");
      if (signedRes[0] !== 0) {
        throw new Error(`issue-5965 sign: ${signRaw.trim()}`);
      }
      if (!existsSync(signed)) {
        throw new Error("issue-5965: signed file was not written");
      }
      console.log("  signed from the Windows store ✓");
      verifySignedPdf(signed);
    });
  } finally {
    removeTestCert(thumb);
    rmSync(signed, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
