// Throw-away PKI for signature validation tests: CA chains, qualified
// certificates, timestamps, CRLs and PDFs signed with them, plus the app's EU
// Trusted List cache. Pure bun, nothing goes into a certificate store.

import { createHash, generateKeyPairSync, randomBytes, sign as rsaSign, type KeyObject } from "node:crypto";
import { writeFileSync } from "node:fs";
import { join } from "node:path";

const TAG_INTEGER = 0x02;
const TAG_BIT_STRING = 0x03;
const TAG_OCTET_STRING = 0x04;
const TAG_NULL = 0x05;
const TAG_OID = 0x06;
const TAG_UTF8_STRING = 0x0c;
const TAG_GENERALIZED_TIME = 0x18;
const TAG_UTC_TIME = 0x17;
const TAG_BOOLEAN = 0x01;
const TAG_SEQUENCE = 0x30;
const TAG_SET = 0x31;
const TAG_CONTEXT = 0xa0;

function tlv(tag: number, ...parts: Uint8Array[]): Buffer {
  const body = Buffer.concat(parts);
  if (body.length < 0x80) {
    return Buffer.concat([Buffer.from([tag, body.length]), body]);
  }
  const lenBytes: number[] = [];
  for (let n = body.length; n > 0; n = Math.floor(n / 256)) {
    lenBytes.unshift(n & 0xff);
  }
  return Buffer.concat([Buffer.from([tag, 0x80 | lenBytes.length, ...lenBytes]), body]);
}

const seq = (...parts: Uint8Array[]) => tlv(TAG_SEQUENCE, ...parts);
const set = (...parts: Uint8Array[]) => tlv(TAG_SET, ...parts);
const octets = (data: Uint8Array) => tlv(TAG_OCTET_STRING, data);
const ctx = (n: number, ...parts: Uint8Array[]) => tlv(TAG_CONTEXT | n, ...parts);
const derNull = () => tlv(TAG_NULL);
const derTrue = () => tlv(TAG_BOOLEAN, Buffer.from([0xff]));

function int(n: number): Buffer {
  const bytes: number[] = [];
  do {
    bytes.unshift(n & 0xff);
    n = Math.floor(n / 256);
  } while (n > 0);
  if (bytes[0]! & 0x80) {
    bytes.unshift(0);
  }
  return tlv(TAG_INTEGER, Buffer.from(bytes));
}

function oid(dotted: string): Buffer {
  const arcs = dotted.split(".").map(Number);
  const bytes = [arcs[0]! * 40 + arcs[1]!];
  for (const arc of arcs.slice(2)) {
    const chunk = [arc & 0x7f];
    for (let n = arc >>> 7; n > 0; n >>>= 7) {
      chunk.unshift((n & 0x7f) | 0x80);
    }
    bytes.push(...chunk);
  }
  return tlv(TAG_OID, Buffer.from(bytes));
}

function utcTime(d: Date): Buffer {
  return tlv(TAG_UTC_TIME, Buffer.from(d.toISOString().replace(/[-:T]/g, "").slice(2, 14) + "Z", "ascii"));
}

function generalizedTime(d: Date): Buffer {
  return tlv(TAG_GENERALIZED_TIME, Buffer.from(d.toISOString().replace(/[-:T]/g, "").slice(0, 14) + "Z", "ascii"));
}

const OID_COMMON_NAME = "2.5.4.3";
const OID_BASIC_CONSTRAINTS = "2.5.29.19";
const OID_KEY_USAGE = "2.5.29.15";
const OID_EXT_KEY_USAGE = "2.5.29.37";
const OID_EKU_TIMESTAMPING = "1.3.6.1.5.5.7.3.8";
const OID_QC_STATEMENTS = "1.3.6.1.5.5.7.1.3";
const OID_QC_COMPLIANCE = "0.4.0.1862.1.1";
const OID_QC_SSCD = "0.4.0.1862.1.4";
const OID_QC_TYPE = "0.4.0.1862.1.6";
const OID_QC_TYPE_ESIGN = "0.4.0.1862.1.6.1";
const OID_QC_TYPE_ESEAL = "0.4.0.1862.1.6.2";
const OID_SHA256 = "2.16.840.1.101.3.4.2.1";
const OID_SHA256_RSA = "1.2.840.113549.1.1.11";
const OID_PKCS7_DATA = "1.2.840.113549.1.7.1";
const OID_PKCS7_SIGNED_DATA = "1.2.840.113549.1.7.2";
const OID_CONTENT_TYPE = "1.2.840.113549.1.9.3";
const OID_MESSAGE_DIGEST = "1.2.840.113549.1.9.4";
const OID_SIGNING_TIME = "1.2.840.113549.1.9.5";
const OID_TST_INFO = "1.2.840.113549.1.9.16.1.4";
const OID_TIMESTAMP_TOKEN = "1.2.840.113549.1.9.16.2.14";
const OID_TSA_POLICY = "1.2.3.4.1";

const DAY_MS = 24 * 60 * 60 * 1000;
const RSA_BITS = 2048;
// keyCertSign + cRLSign, and digitalSignature + nonRepudiation
const KEY_USAGE_CA = [1, 0x06];
const KEY_USAGE_SIGNER = [6, 0xc0];

export const days = (n: number) => new Date(Date.now() + n * DAY_MS);

function nameOf(commonName: string): Buffer {
  return seq(set(seq(oid(OID_COMMON_NAME), tlv(TAG_UTF8_STRING, Buffer.from(commonName, "utf8")))));
}

export type Qualified = "esign" | "eseal" | "no-sscd";

export type CertSpec = {
  name: string;
  issuer?: Cert;
  isCa?: boolean;
  isTsa?: boolean;
  qualified?: Qualified;
  notBefore?: Date;
  notAfter?: Date;
};

export type Cert = {
  name: string;
  der: Buffer;
  serial: Buffer;
  publicKey: KeyObject;
  privateKey: KeyObject;
};

function qcStatements(q: Qualified): Buffer {
  const statements = [seq(oid(OID_QC_COMPLIANCE))];
  if (q !== "no-sscd") {
    statements.push(seq(oid(OID_QC_SSCD)));
  }
  const type = q === "eseal" ? OID_QC_TYPE_ESEAL : OID_QC_TYPE_ESIGN;
  statements.push(seq(oid(OID_QC_TYPE), seq(oid(type))));
  return seq(oid(OID_QC_STATEMENTS), octets(seq(...statements)));
}

export function makeCert(spec: CertSpec): Cert {
  const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: RSA_BITS });
  const sigAlg = seq(oid(OID_SHA256_RSA), derNull());
  const subject = nameOf(spec.name);
  const issuerName = spec.issuer ? nameOf(spec.issuer.name) : subject;
  const signingKey = spec.issuer ? spec.issuer.privateKey : privateKey;
  // the top bit clear so it reads as positive
  const serial = tlv(TAG_INTEGER, Buffer.concat([Buffer.from([1]), randomBytes(8)]));

  const exts: Buffer[] = [];
  if (spec.isCa) {
    exts.push(seq(oid(OID_BASIC_CONSTRAINTS), derTrue(), octets(seq(derTrue()))));
    exts.push(seq(oid(OID_KEY_USAGE), derTrue(), octets(tlv(TAG_BIT_STRING, Buffer.from(KEY_USAGE_CA)))));
  } else {
    exts.push(seq(oid(OID_KEY_USAGE), derTrue(), octets(tlv(TAG_BIT_STRING, Buffer.from(KEY_USAGE_SIGNER)))));
  }
  if (spec.isTsa) {
    exts.push(seq(oid(OID_EXT_KEY_USAGE), derTrue(), octets(seq(oid(OID_EKU_TIMESTAMPING)))));
  }
  if (spec.qualified) {
    exts.push(qcStatements(spec.qualified));
  }

  const tbs = seq(
    ctx(0, int(2)),
    serial,
    sigAlg,
    issuerName,
    seq(utcTime(spec.notBefore ?? days(-30)), utcTime(spec.notAfter ?? days(365))),
    subject,
    publicKey.export({ type: "spki", format: "der" }),
    ctx(3, seq(...exts)),
  );
  const sig = rsaSign("sha256", tbs, signingKey);
  return {
    name: spec.name,
    der: seq(tbs, sigAlg, tlv(TAG_BIT_STRING, Buffer.from([0]), sig)),
    serial,
    publicKey,
    privateKey,
  };
}

const sha256 = (data: Uint8Array) => createHash("sha256").update(data).digest();

function attr(type: string, value: Uint8Array): Buffer {
  return seq(oid(type), set(value));
}

// a DER SET has its elements sorted by encoding
function sortedAttrs(attrs: Buffer[]): Buffer[] {
  return [...attrs].sort(Buffer.compare);
}

type SignedDataSpec = {
  signer: Cert;
  certs: Cert[];
  contentType: string;
  digest: Buffer;
  // encapsulated content; a PDF signature is detached
  content?: Buffer;
  signingTime?: Date;
  unsignedAttrs?: Buffer[];
};

function makeSignedData(spec: SignedDataSpec): { cms: Buffer; signature: Buffer } {
  const digestAlg = seq(oid(OID_SHA256), derNull());
  const signed = sortedAttrs([
    attr(OID_CONTENT_TYPE, oid(spec.contentType)),
    attr(OID_MESSAGE_DIGEST, octets(spec.digest)),
    attr(OID_SIGNING_TIME, utcTime(spec.signingTime ?? new Date())),
  ]);
  // signed as a SET, stored as [0]
  const signature = rsaSign("sha256", set(...signed), spec.signer.privateKey);
  const signerInfoParts = [
    int(1),
    seq(issuerOf(spec.signer), spec.signer.serial),
    digestAlg,
    ctx(0, ...signed),
    seq(oid(OID_SHA256_RSA), derNull()),
    octets(signature),
  ];
  if (spec.unsignedAttrs && spec.unsignedAttrs.length > 0) {
    signerInfoParts.push(ctx(1, ...spec.unsignedAttrs));
  }
  const encap = spec.content ? seq(oid(spec.contentType), ctx(0, octets(spec.content))) : seq(oid(spec.contentType));
  const signedData = seq(
    int(spec.content ? 3 : 1),
    set(digestAlg),
    encap,
    ctx(0, ...spec.certs.map((c) => c.der)),
    set(seq(...signerInfoParts)),
  );
  return { cms: seq(oid(OID_PKCS7_SIGNED_DATA), ctx(0, signedData)), signature };
}

const gIssuerNames = new WeakMap<Cert, Buffer>();

function issuerOf(c: Cert): Buffer {
  const known = gIssuerNames.get(c);
  if (known) {
    return known;
  }
  // tbs is the certificate's first element: version, serial, sigAlg, issuer
  const tbs = readDer(readDer(c.der, 0).body, 0);
  let pos = 0;
  let node = readDer(tbs.body, pos);
  for (let i = 0; i < 3; i++) {
    pos += node.raw.length;
    node = readDer(tbs.body, pos);
  }
  gIssuerNames.set(c, node.raw);
  return node.raw;
}

function readDer(buf: Buffer, pos: number): { body: Buffer; raw: Buffer } {
  let len = buf[pos + 1]!;
  let hdr = 2;
  if (len & 0x80) {
    const n = len & 0x7f;
    len = 0;
    for (let i = 0; i < n; i++) {
      len = len * 256 + buf[pos + 2 + i]!;
    }
    hdr += n;
  }
  return { body: buf.subarray(pos + hdr, pos + hdr + len), raw: buf.subarray(pos, pos + hdr + len) };
}

// RFC 3161 token over a signature value, as a TSA issues it
function makeTimestampToken(tsa: Cert, chain: Cert[], imprintOf: Buffer, genTime: Date): Buffer {
  const tstInfo = seq(
    int(1),
    oid(OID_TSA_POLICY),
    seq(seq(oid(OID_SHA256), derNull()), octets(imprintOf)),
    int(1234),
    generalizedTime(genTime),
  );
  return makeSignedData({
    signer: tsa,
    certs: chain,
    contentType: OID_TST_INFO,
    digest: sha256(tstInfo),
    content: tstInfo,
    signingTime: genTime,
  }).cms;
}

export type PdfSignSpec = {
  signer: Cert;
  // what the signature carries besides the signer; the chain's top is usually left out
  extraCerts?: Cert[];
  signingTime?: Date;
  timestamp?: { tsa: Cert; chain: Cert[]; time: Date; forgeImprint?: boolean };
};

const CONTENTS_HEX_LEN = 32000;
const BYTE_RANGE_PLACEHOLDER = "0 0000000000 0000000000 0000000000";

function pdfDate(d: Date): string {
  return "D:" + d.toISOString().replace(/[-:T]/g, "").slice(0, 14) + "Z";
}

function assemblePdf(sigDict: string): Buffer {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /SigFlags 3 /Fields [4 0 R] >> >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 300] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Widget /FT /Sig /T (Signature1) /Rect [0 0 0 0] /F 4 /P 3 0 R /V 5 0 R >>",
    sigDict,
  ];
  let body = "%PDF-1.7\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(Buffer.byteLength(body, "latin1"));
    body += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = Buffer.byteLength(body, "latin1");
  body += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  body += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(body, "latin1");
}

// Writes a one-page PDF with a PAdES signature made directly with these keys.
export function writeSignedPdf(path: string, spec: PdfSignSpec): void {
  const signingTime = spec.signingTime ?? new Date();
  const placeholder = "0".repeat(CONTENTS_HEX_LEN);
  const dict = (range: string, contents: string) =>
    `<< /Type /Sig /Filter /Adobe.PPKLite /SubFilter /ETSI.CAdES.detached /M (${pdfDate(signingTime)}) ` +
    `/ByteRange [${range}] /Contents <${contents}> >>`;

  let pdf = assemblePdf(dict(BYTE_RANGE_PLACEHOLDER, placeholder));
  const contentsAt = pdf.indexOf(`<${placeholder}>`);
  const afterContents = contentsAt + placeholder.length + 2;
  const fmt = (n: number) => String(n).padStart(10, "0");
  const range = `0 ${fmt(contentsAt)} ${fmt(afterContents)} ${fmt(pdf.length - afterContents)}`;
  pdf = assemblePdf(dict(range, placeholder));
  const signedBytes = Buffer.concat([pdf.subarray(0, contentsAt), pdf.subarray(afterContents)]);

  const certs = [spec.signer, ...(spec.extraCerts ?? [])];
  const sign = (unsignedAttrs?: Buffer[]) =>
    makeSignedData({
      signer: spec.signer,
      certs,
      contentType: OID_PKCS7_DATA,
      digest: sha256(signedBytes),
      signingTime,
      unsignedAttrs,
    });

  let cms = sign().cms;
  if (spec.timestamp) {
    const { tsa, chain, time, forgeImprint } = spec.timestamp;
    const imprint = forgeImprint ? sha256(Buffer.from("not the signature")) : sha256(sign().signature);
    const token = makeTimestampToken(tsa, chain, imprint, time);
    cms = sign([attr(OID_TIMESTAMP_TOKEN, token)]).cms;
  }
  const hex = cms.toString("hex");
  if (hex.length > CONTENTS_HEX_LEN) {
    throw new Error("the signature doesn't fit the /Contents placeholder");
  }
  pdf.write(hex, contentsAt + 1, "latin1");
  writeFileSync(path, pdf);
}

// --- the app's EU Trusted List cache ---------------------------------------

export type EutlKind = "CA" | "TSA";

export type EutlEntry = {
  cert: Cert;
  kind?: EutlKind;
  // periods the service was granted; none means listed, but never qualified
  granted?: { from: Date; to?: Date; esig?: boolean; eseal?: boolean }[];
};

const NEVER = 0;

function unix(d: Date): number {
  return Math.floor(d.getTime() / 1000);
}

// Same format EutlUpdate() writes to the app data directory.
export function writeEutlCache(appdata: string, entries: EutlEntry[]): void {
  const lines = [`# sumatrapdf eutl 2 ${new Date().toISOString().slice(0, 10)}`];
  for (const e of entries) {
    const hash = sha256(e.cert.der).toString("hex");
    lines.push(`C ${hash} ${e.cert.der.toString("base64")}`);
    for (const g of e.granted ?? []) {
      const end = g.to ? unix(g.to) : NEVER;
      lines.push(
        `S ${hash} ${e.kind ?? "CA"} ${unix(g.from)} ${end} ${g.esig === false ? 0 : 1} ${g.eseal === false ? 0 : 1}`,
      );
    }
  }
  writeFileSync(join(appdata, "eutl-services.txt"), lines.join("\n") + "\n");
}
