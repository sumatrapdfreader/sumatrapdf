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

import { existsSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { EXE, ROOT, runStandalone, tmpPath, writePdfWithEmptySigField, removeTestCert, runPowerShell } from "./util.ts";

const kCertSubject = "CN=SumatraPDF StoreSignTest";

function findToolExe(): string | null {
  const candidates = [
    join(dirname(EXE), "sumatrapdf-tool.exe"),
    join(ROOT, "out", "dbg64", "sumatrapdf-tool.exe"),
    join(ROOT, "out", "rel64", "sumatrapdf-tool.exe"),
  ];
  return candidates.find((p) => existsSync(p)) ?? null;
}

// Uses .NET CertificateRequest so it works even when the Cert: PSDrive is missing
// (New-SelfSignedCertificate needs that drive). PersistKeySet keeps the private
// key in the user store after this process exits.
function makeTestCert(): string | null {
  const created = runPowerShell(`
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
  if (IS_MAC) {
    console.log("SKIP issue-5965: signing uses the Windows certificate store");
    return;
  }
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
