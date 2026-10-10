// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5581
//
// Document Properties for a digitally signed PDF must report the hash and
// signature algorithms, issuer, expiry / LTV, device vs TSA time labels, and
// the Windows (or EUTL) trust source. This signs a tiny PDF with a throw-away
// store cert (same setup as issue-5965) and reads the signatures property.
//
// Skips if this machine cannot create a test certificate.
//
// Run: bun tests/issue-5581.ts [--no-build]

import { existsSync, rmSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { EXE, ROOT, runStandalone, tmpPath, writePdfWithEmptySigField, removeTestCert, runPowerShell } from "./util.ts";

const kCertSubject = "CN=SumatraPDF SigPropsTest";

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
    console.log(`\nSKIP issue-5581: could not create a test certificate:\n${created.out}`);
    return null;
  }
  return thumb.toUpperCase();
}

function requireLine(raw: string, re: RegExp, label: string): void {
  if (!re.test(raw)) {
    throw new Error(`issue-5581: missing ${label}:\n${raw}`);
  }
}

async function signaturesOf(pdf: string): Promise<string> {
  return withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 20_000;
      let raw = "";
      for (;;) {
        const res = await client.request(ControlCommand.TestDocumentSignatures);
        const code = res[0] as number;
        raw = String(res[1] ?? "");
        if (code === 0) {
          return raw;
        }
        if (code !== 2) {
          throw new Error(`issue-5581 signatures: ${raw.trim()}`);
        }
        if (Date.now() > deadline) {
          throw new Error(`issue-5581: signatures never ready: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 150));
      }
    },
    [pdf],
  );
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-5581: signature properties use the Windows PKCS#7 verifier");
    return;
  }
  const padesDir = join(ROOT, "tests", "issue-5581-data");
  const bt = join(padesDir, "test_sign_PAdES_B-T.pdf");
  const lta = join(padesDir, "test_sign_PAdES_B-LTA.pdf");
  if (existsSync(bt)) {
    const raw = await signaturesOf(bt);
    requireLine(raw, /Included Time Stamp/, "embedded TSA block");
    requireLine(raw, /Sectigo Public Time Stamping/, "TSA signer");
    requireLine(raw, /PAdES B-T/, "PAdES B-T level");
    requireLine(raw, /The time and date displayed is from the secure time/, "TSA time label");
    console.log(`issue-5581 B-T:\n${raw.trim()}`);
  }
  if (existsSync(lta)) {
    const raw = await signaturesOf(lta);
    requireLine(raw, /Included Time Stamp/, "embedded TSA on the CAdES signature");
    requireLine(raw, /Signature 2 \(document timestamp\)/, "document timestamp field");
    requireLine(raw, /Sectigo Public Time Stamping/, "TSA certificate");
    if (/Signature 2[\s\S]*not signed/.test(raw)) {
      throw new Error(`issue-5581: document timestamp treated as unsigned:\n${raw}`);
    }
    console.log(`issue-5581 B-LTA:\n${raw.trim()}`);
  }

  const thumb = makeTestCert();
  if (!thumb) {
    return;
  }
  const pdf = tmpPath("issue-5581.pdf");
  const signed = tmpPath("issue-5581-signed.pdf");
  writePdfWithEmptySigField(pdf);
  rmSync(signed, { force: true });

  try {
    await withControlledSumatra(EXE, async (client) => {
      const signedRes = await client.request(ControlCommand.TestSignDocument, [pdf, signed, thumb]);
      if (signedRes[0] !== 0) {
        throw new Error(`issue-5581 sign: ${String(signedRes[1] ?? "").trim()}`);
      }
      if (!existsSync(signed)) {
        throw new Error("issue-5581: signed file was not written");
      }
    });

    await withControlledSumatra(
      EXE,
      async (client) => {
        const deadline = Date.now() + 20_000;
        let raw = "";
        for (;;) {
          const res = await client.request(ControlCommand.TestDocumentSignatures);
          const code = res[0] as number;
          raw = String(res[1] ?? "");
          if (code === 0) {
            break;
          }
          if (code !== 2) {
            throw new Error(`issue-5581 signatures: ${raw.trim()}`);
          }
          if (Date.now() > deadline) {
            throw new Error(`issue-5581: signatures never ready: ${raw.trim()}`);
          }
          await new Promise((r) => setTimeout(r, 150));
        }
        requireLine(raw, /Signed by:/, "Signed by");
        requireLine(raw, /Source of trust: Windows Certificate Store/, "Windows trust source");
        requireLine(raw, /Hash algorithm: SHA/i, "hash algorithm");
        requireLine(raw, /Signature algorithm:/, "signature algorithm");
        requireLine(raw, /Certificate issued by:/i, "issuer");
        requireLine(raw, /The time and date displayed is from the user device/, "device-time label");
        requireLine(raw, /wasn't changed since the signature was applied|document unchanged/i, "digest status");
        console.log(`issue-5581:\n${raw.trim()}`);
      },
      [signed],
    );
  } finally {
    removeTestCert(thumb);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
