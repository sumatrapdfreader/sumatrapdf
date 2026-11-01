// Validating digital signatures in Properties: trust through the EU Trusted
// List, whether a signature is qualified, and the time it is checked at.
//
// The PKI is generated, and the signed PDFs are built directly, so nothing goes
// into a certificate store and no network is needed. See pki-chain.ts.

import { rmSync } from "node:fs";
import { IS_MAC } from "./host.ts";
import { ControlCommand, type ControlClient, withControlledSumatra } from "./control.ts";
import {
  days,
  makeCert,
  writeEutlCache,
  writeSignedPdf,
  type Cert,
  type EutlEntry,
  type PdfSignSpec,
} from "./pki-chain.ts";
import { EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";

const SIGNATURES_READY_MS = 20_000;
const NOT_READY = 2;
const SETTINGS = "RestoreSession = false\nCheckForUpdates = false\n";

const QUALIFIED_SIG = /This is a Qualified Electronic Signature according to EU Regulation 910\/2014/;
const QUALIFIED_SEAL = /This is a Qualified Electronic Seal according to EU Regulation 910\/2014/;
const ADVANCED_ON_QC = /advanced electronic signature with a qualified certificate/;
const CLAIM_ONLY = /The certificate says it is qualified, but/;
const EU_SOURCE = /Source of trust: European Union Trusted List \(EUTL\)/;
const NO_SOURCE = /Source of trust: none/;
const UNTRUSTED = /Certificate: /;

async function signaturesOf(client: ControlClient): Promise<string> {
  const deadline = Date.now() + SIGNATURES_READY_MS;
  for (;;) {
    const res = await client.request(ControlCommand.TestDocumentSignatures);
    const raw = String(res[1] ?? "");
    if (res[0] === 0) {
      return raw;
    }
    if (res[0] !== NOT_READY) {
      throw new Error(`sig-validation: ${raw.trim()}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`sig-validation: signatures never ready: ${raw.trim()}`);
    }
    await Bun.sleep(100);
  }
}

let gCase = 0;

// Properties' text for a PDF signed as spec says, with this EU list on disk.
async function propsFor(spec: PdfSignSpec, list: EutlEntry[] | null): Promise<string> {
  gCase++;
  const pdf = tmpPath(`sig-validation-${gCase}.pdf`);
  writeSignedPdf(pdf, spec);
  const appdata = writeAppdata(`sig-validation-${gCase}`, SETTINGS);
  rmSync(`${appdata}/eutl-services.txt`, { force: true });
  if (list) {
    writeEutlCache(appdata, list);
  }
  return withControlledSumatra(EXE, signaturesOf, ["-appdata", appdata, pdf]);
}

function expectHas(raw: string, re: RegExp, what: string): void {
  if (!re.test(raw)) {
    throw new Error(`sig-validation: ${what}: no ${re}\n${raw}`);
  }
}

function expectNot(raw: string, re: RegExp, what: string): void {
  if (re.test(raw)) {
    throw new Error(`sig-validation: ${what}: has ${re}\n${raw}`);
  }
}

type Pki = { root: Cert; ca: Cert; leaf: Cert };

function makePki(q?: "esign" | "eseal" | "no-sscd"): Pki {
  const root = makeCert({ name: "Test EU Root", isCa: true, notAfter: days(3000) });
  const ca = makeCert({ name: "Test Qualified CA", issuer: root, isCa: true, notAfter: days(2000) });
  const leaf = makeCert({ name: "Jan Kowalski", issuer: ca, qualified: q });
  return { root, ca, leaf };
}

const LONG_AGO = days(-1000);

async function testTrustedList(): Promise<void> {
  const { ca, leaf } = makePki("esign");
  const granted: EutlEntry = { cert: ca, granted: [{ from: LONG_AGO }] };

  let raw = await propsFor({ signer: leaf, extraCerts: [ca] }, [granted]);
  expectHas(raw, QUALIFIED_SIG, "qualified certificate from a listed CA");
  expectHas(raw, EU_SOURCE, "qualified certificate from a listed CA");
  expectNot(raw, UNTRUSTED, "qualified certificate from a listed CA");

  // the list supplies a CA the signature doesn't carry
  raw = await propsFor({ signer: leaf }, [granted]);
  expectHas(raw, QUALIFIED_SIG, "issuer only on the list");

  raw = await propsFor({ signer: leaf, extraCerts: [ca] }, null);
  expectHas(raw, /EU Trusted List isn't downloaded/, "no list");
  expectHas(raw, NO_SOURCE, "no list");
  expectHas(raw, UNTRUSTED, "no list");

  raw = await propsFor({ signer: leaf, extraCerts: [ca] }, [{ cert: makePki().ca, granted: [{ from: LONG_AGO }] }]);
  expectHas(raw, CLAIM_ONLY, "issuer isn't listed");
  expectHas(raw, NO_SOURCE, "issuer isn't listed");

  // listed, but the service was withdrawn before this signature
  raw = await propsFor({ signer: leaf, extraCerts: [ca] }, [
    { cert: ca, granted: [{ from: LONG_AGO, to: days(-10) }] },
  ]);
  expectHas(raw, CLAIM_ONLY, "withdrawn service");

  // a TSA service doesn't vouch for a signing certificate
  raw = await propsFor({ signer: leaf, extraCerts: [ca] }, [{ cert: ca, kind: "TSA", granted: [{ from: LONG_AGO }] }]);
  expectHas(raw, CLAIM_ONLY, "listed only as a TSA");

  // a service for seals doesn't make a signature qualified
  raw = await propsFor({ signer: leaf, extraCerts: [ca] }, [{ cert: ca, granted: [{ from: LONG_AGO, esig: false }] }]);
  expectHas(raw, CLAIM_ONLY, "listed for seals only");

  const seal = makePki("eseal");
  raw = await propsFor({ signer: seal.leaf, extraCerts: [seal.ca] }, [
    { cert: seal.ca, granted: [{ from: LONG_AGO }] },
  ]);
  expectHas(raw, QUALIFIED_SEAL, "qualified seal");

  const noDevice = makePki("no-sscd");
  raw = await propsFor({ signer: noDevice.leaf, extraCerts: [noDevice.ca] }, [
    { cert: noDevice.ca, granted: [{ from: LONG_AGO }] },
  ]);
  expectHas(raw, ADVANCED_ON_QC, "no qualified device");
  expectNot(raw, QUALIFIED_SIG, "no qualified device");

  // trusted through the list, but not a qualified certificate
  const plain = makePki();
  raw = await propsFor({ signer: plain.leaf, extraCerts: [plain.ca] }, [
    { cert: plain.ca, granted: [{ from: LONG_AGO }] },
  ]);
  expectNot(raw, QUALIFIED_SIG, "not a qualified certificate");
  expectNot(raw, CLAIM_ONLY, "not a qualified certificate");

  // the signer's own certificate on the list proves nothing about its issuer
  const self = makeCert({ name: "Self Signed", qualified: "esign" });
  raw = await propsFor({ signer: self }, [{ cert: self, granted: [{ from: LONG_AGO }] }]);
  expectNot(raw, QUALIFIED_SIG, "self-signed and listed");
  expectHas(raw, NO_SOURCE, "self-signed and listed");
}

const AS_OF_TIMESTAMP = /validated as of the secure \(timestamp\) time/;
const AS_OF_NOW = /validated as of the current time/;
const EXPIRED_NO_PROOF = /isn't valid at that time, and no trusted timestamp proves/;
const BAD_TIMESTAMP = /This timestamp couldn't be verified/;

async function testSigningTime(): Promise<void> {
  const root = makeCert({ name: "Test EU Root", isCa: true, notBefore: days(-3000), notAfter: days(3000) });
  const ca = makeCert({
    name: "Test Qualified CA",
    issuer: root,
    isCa: true,
    notBefore: days(-2000),
    notAfter: days(2000),
  });
  const tsa = makeCert({ name: "Test TSA", issuer: ca, isTsa: true, notBefore: days(-2000), notAfter: days(2000) });
  const expired = makeCert({
    name: "Jan Kowalski",
    issuer: ca,
    qualified: "esign",
    notBefore: days(-400),
    notAfter: days(-10),
  });
  const signedAt = days(-100);
  const listCa: EutlEntry = { cert: ca, granted: [{ from: days(-1500) }] };
  const listTsa: EutlEntry = { cert: ca, kind: "TSA", granted: [{ from: days(-1500) }] };
  const spec = { signer: expired, extraCerts: [ca], signingTime: signedAt };
  const stamp = { tsa, chain: [tsa, ca], time: signedAt };

  // expired since, but a trusted timestamp proves it was valid when signed
  let raw = await propsFor({ ...spec, timestamp: stamp }, [listCa, listTsa]);
  expectHas(raw, QUALIFIED_SIG, "expired, with a timestamp");
  expectHas(raw, AS_OF_TIMESTAMP, "expired, with a timestamp");
  expectNot(raw, UNTRUSTED, "expired, with a timestamp");

  // without proof the signer's clock counts for nothing
  raw = await propsFor(spec, [listCa, listTsa]);
  expectHas(raw, AS_OF_NOW, "expired, no timestamp");
  expectHas(raw, EXPIRED_NO_PROOF, "expired, no timestamp");
  expectHas(raw, UNTRUSTED, "expired, no timestamp");
  expectNot(raw, QUALIFIED_SIG, "expired, no timestamp");

  // a timestamp of something else
  raw = await propsFor({ ...spec, timestamp: { ...stamp, forgeImprint: true } }, [listCa, listTsa]);
  expectHas(raw, BAD_TIMESTAMP, "forged timestamp");
  expectHas(raw, EXPIRED_NO_PROOF, "forged timestamp");

  // a timestamp nobody vouches for
  const stranger = makeCert({ name: "Unknown TSA", isTsa: true, notBefore: days(-2000), notAfter: days(2000) });
  raw = await propsFor({ ...spec, timestamp: { tsa: stranger, chain: [stranger], time: signedAt } }, [listCa, listTsa]);
  expectHas(raw, BAD_TIMESTAMP, "untrusted TSA");
  expectHas(raw, EXPIRED_NO_PROOF, "untrusted TSA");

  // what counts is the service's status when signed, not today
  raw = await propsFor({ ...spec, timestamp: stamp }, [
    { cert: ca, granted: [{ from: days(-1500), to: days(-50) }] },
    listTsa,
  ]);
  expectHas(raw, QUALIFIED_SIG, "withdrawn after signing");
  raw = await propsFor({ ...spec, timestamp: stamp }, [
    { cert: ca, granted: [{ from: days(-1500), to: days(-200) }] },
    listTsa,
  ]);
  expectHas(raw, CLAIM_ONLY, "withdrawn before signing");
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP sig-validation: the checks are in the Windows PKCS#7 backend");
    return;
  }
  await testTrustedList();
  await testSigningTime();
}

if (import.meta.main) {
  await runStandalone(testit);
}
