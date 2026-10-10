// A throw-away signing certificate and a PDF signature check, in pure bun:
// no PowerShell PKI cmdlets, no openssl binary, nothing left in a cert store.

import {
  createCipheriv,
  createHash,
  createHmac,
  generateKeyPairSync,
  randomBytes,
  sign as rsaSign,
  verify as rsaVerify,
  type KeyObject,
} from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";

// --- DER -------------------------------------------------------------------

const TAG_INTEGER = 0x02;
const TAG_BIT_STRING = 0x03;
const TAG_OCTET_STRING = 0x04;
const TAG_NULL = 0x05;
const TAG_OID = 0x06;
const TAG_UTF8_STRING = 0x0c;
const TAG_UTC_TIME = 0x17;
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

function int(n: number): Buffer {
  const bytes: number[] = [];
  do {
    bytes.unshift(n & 0xff);
    n = Math.floor(n / 256);
  } while (n > 0);
  // a set top bit would read as negative
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
  const s = d.toISOString().replace(/[-:T]/g, "").slice(2, 14) + "Z";
  return tlv(TAG_UTC_TIME, Buffer.from(s, "ascii"));
}

const OID_COMMON_NAME = "2.5.4.3";
const OID_KEY_USAGE = "2.5.29.15";
const OID_SHA1 = "1.3.14.3.2.26";
const OID_SHA256 = "2.16.840.1.101.3.4.2.1";
const OID_SHA384 = "2.16.840.1.101.3.4.2.2";
const OID_SHA512 = "2.16.840.1.101.3.4.2.3";
const OID_SHA256_RSA = "1.2.840.113549.1.1.11";
const OID_PKCS7_DATA = "1.2.840.113549.1.7.1";
const OID_PKCS7_SIGNED_DATA = "1.2.840.113549.1.7.2";
const OID_MESSAGE_DIGEST = "1.2.840.113549.1.9.4";
const OID_LOCAL_KEY_ID = "1.2.840.113549.1.9.21";
const OID_X509_CERT = "1.2.840.113549.1.9.22.1";
const OID_PBE_SHA1_3DES = "1.2.840.113549.1.12.1.3";
const OID_SHROUDED_KEY_BAG = "1.2.840.113549.1.12.10.1.2";
const OID_CERT_BAG = "1.2.840.113549.1.12.10.1.3";

// --- certificate -----------------------------------------------------------

const DAY_MS = 24 * 60 * 60 * 1000;
const CERT_VALID_DAYS = 365;
const RSA_BITS = 2048;

function makeSelfSignedCert(commonName: string, publicKey: KeyObject, privateKey: KeyObject): Buffer {
  const name = seq(set(seq(oid(OID_COMMON_NAME), tlv(TAG_UTF8_STRING, Buffer.from(commonName, "utf8")))));
  const sigAlg = seq(oid(OID_SHA256_RSA), derNull());
  const now = Date.now();
  // digitalSignature only: one BIT STRING byte, 7 unused bits
  const keyUsage = seq(oid(OID_KEY_USAGE), octets(tlv(TAG_BIT_STRING, Buffer.from([7, 0x80]))));
  const tbs = seq(
    ctx(0, int(2)), // v3
    tlv(TAG_INTEGER, Buffer.concat([Buffer.from([1]), randomBytes(8)])),
    sigAlg,
    name,
    seq(utcTime(new Date(now - DAY_MS)), utcTime(new Date(now + CERT_VALID_DAYS * DAY_MS))),
    name,
    publicKey.export({ type: "spki", format: "der" }),
    ctx(3, seq(keyUsage)),
  );
  const sig = rsaSign("sha256", tbs, privateKey);
  return seq(tbs, sigAlg, tlv(TAG_BIT_STRING, Buffer.from([0]), sig));
}

// --- PKCS#12 ---------------------------------------------------------------

const P12_ID_KEY = 1;
const P12_ID_IV = 2;
const P12_ID_MAC = 3;
const P12_ITERATIONS = 2048;
const P12_SALT_LEN = 8;
const SHA1_LEN = 20;
const SHA1_BLOCK = 64;
const DES3_KEY_LEN = 24;
const DES_IV_LEN = 8;

// RFC 7292 appendix B.2 with SHA-1.
function p12Kdf(password: string, salt: Uint8Array, id: number, iterations: number, size: number): Buffer {
  // BMPString: UTF-16BE with a terminating NUL
  const pw = Buffer.from(password + "\0", "utf16le").swap16();
  const fill = (src: Uint8Array) => {
    const out = Buffer.alloc(SHA1_BLOCK * Math.ceil(src.length / SHA1_BLOCK));
    for (let i = 0; i < out.length; i++) {
      out[i] = src[i % src.length]!;
    }
    return out;
  };
  const diversifier = Buffer.alloc(SHA1_BLOCK, id);
  const input = Buffer.concat([fill(salt), fill(pw)]);
  const out: Buffer[] = [];
  for (let have = 0; have < size; have += SHA1_LEN) {
    let a = createHash("sha1").update(diversifier).update(input).digest();
    for (let i = 1; i < iterations; i++) {
      a = createHash("sha1").update(a).digest();
    }
    out.push(a);

    // input blocks += B + 1, each a big-endian number
    const b = fill(a);
    for (let blk = 0; blk < input.length; blk += SHA1_BLOCK) {
      let carry = 1;
      for (let i = SHA1_BLOCK - 1; i >= 0; i--) {
        carry += input[blk + i]! + b[i]!;
        input[blk + i] = carry & 0xff;
        carry >>= 8;
      }
    }
  }
  return Buffer.concat(out).subarray(0, size);
}

function makePfx(certDer: Buffer, privateKey: KeyObject, password: string): Buffer {
  const keyId = seq(oid(OID_LOCAL_KEY_ID), set(octets(createHash("sha1").update(certDer).digest())));

  const certBag = seq(oid(OID_CERT_BAG), ctx(0, seq(oid(OID_X509_CERT), ctx(0, octets(certDer)))), set(keyId));

  const keySalt = randomBytes(P12_SALT_LEN);
  const key = p12Kdf(password, keySalt, P12_ID_KEY, P12_ITERATIONS, DES3_KEY_LEN);
  const iv = p12Kdf(password, keySalt, P12_ID_IV, P12_ITERATIONS, DES_IV_LEN);
  const cipher = createCipheriv("des-ede3-cbc", key, iv);
  const pkcs8 = privateKey.export({ type: "pkcs8", format: "der" });
  const encKey = Buffer.concat([cipher.update(pkcs8), cipher.final()]);
  const pbe = seq(oid(OID_PBE_SHA1_3DES), seq(octets(keySalt), int(P12_ITERATIONS)));
  const keyBag = seq(oid(OID_SHROUDED_KEY_BAG), ctx(0, seq(pbe, octets(encKey))), set(keyId));

  const dataInfo = (content: Uint8Array) => seq(oid(OID_PKCS7_DATA), ctx(0, octets(content)));
  const authSafe = seq(dataInfo(seq(certBag)), dataInfo(seq(keyBag)));

  const macSalt = randomBytes(P12_SALT_LEN);
  const macKey = p12Kdf(password, macSalt, P12_ID_MAC, P12_ITERATIONS, SHA1_LEN);
  const mac = createHmac("sha1", macKey).update(authSafe).digest();
  const macData = seq(seq(seq(oid(OID_SHA1), derNull()), octets(mac)), octets(macSalt), int(P12_ITERATIONS));

  return seq(int(3), dataInfo(authSafe), macData);
}

export type TestCert = { certDer: Buffer; publicKey: KeyObject };

// Writes a self-signed RSA signing certificate with its key to a .pfx.
export function writeTestPfx(pfxPath: string, commonName: string, password: string): TestCert {
  const { publicKey, privateKey } = generateKeyPairSync("rsa", { modulusLength: RSA_BITS });
  const certDer = makeSelfSignedCert(commonName, publicKey, privateKey);
  writeFileSync(pfxPath, makePfx(certDer, privateKey, password));
  return { certDer, publicKey };
}

// --- signature check -------------------------------------------------------

type Node = { tag: number; start: number; body: Buffer; raw: Buffer };

function readNode(buf: Buffer, pos: number): Node {
  const tag = buf[pos]!;
  let len = buf[pos + 1]!;
  let hdr = 2;
  if (len === 0x80) {
    throw new Error("indefinite-length encoding in the signature");
  }
  if (len & 0x80) {
    const n = len & 0x7f;
    len = 0;
    for (let i = 0; i < n; i++) {
      len = len * 256 + buf[pos + 2 + i]!;
    }
    hdr += n;
  }
  const end = pos + hdr + len;
  if (end > buf.length) {
    throw new Error("truncated DER in the signature");
  }
  return { tag, start: pos, body: buf.subarray(pos + hdr, end), raw: buf.subarray(pos, end) };
}

function children(node: Node): Node[] {
  const res: Node[] = [];
  for (let pos = 0; pos < node.body.length;) {
    const child = readNode(node.body, pos);
    res.push(child);
    pos += child.raw.length;
  }
  return res;
}

const DIGEST_BY_OID = new Map<string, string>([
  [oid(OID_SHA1).toString("hex"), "sha1"],
  [oid(OID_SHA256).toString("hex"), "sha256"],
  [oid(OID_SHA384).toString("hex"), "sha384"],
  [oid(OID_SHA512).toString("hex"), "sha512"],
]);

// Object numbers of the signature fields in the file, across all revisions.
export function pdfSigFieldObjects(pdfPath: string): number[] {
  const text = readFileSync(pdfPath).toString("latin1");
  const res = new Set<number>();
  for (const m of text.matchAll(/(\d+) 0 obj\b([^]*?)endobj/g)) {
    if (/\/FT\s*\/Sig\b/.test(m[2]!)) {
      res.add(Number(m[1]));
    }
  }
  return [...res].sort((a, b) => a - b);
}

// Throws unless the file's last signature was made with cert's key over the
// bytes its /ByteRange names, i.e. the document is unchanged since signing.
export function verifyPdfSignature(pdfPath: string, cert: TestCert): void {
  const pdf = readFileSync(pdfPath);
  const text = pdf.toString("latin1");
  const ranges = [...text.matchAll(/\/ByteRange\s*\[\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*\]/g)];
  const last = ranges[ranges.length - 1];
  if (!last) {
    throw new Error(`${pdfPath} has no signature`);
  }
  const [a, b, c, d] = last.slice(1).map(Number) as [number, number, number, number];
  if (c + d !== pdf.length) {
    throw new Error(`${pdfPath}: the signature doesn't cover the whole file`);
  }
  const signed = Buffer.concat([pdf.subarray(a, a + b), pdf.subarray(c, c + d)]);
  // between the ranges: <hex of the CMS blob, zero padded>
  const hex = text.slice(a + b + 1, c - 1);
  const cms = Buffer.from(hex, "hex");

  const contentInfo = children(readNode(cms, 0));
  if (!contentInfo[0]!.raw.equals(oid(OID_PKCS7_SIGNED_DATA))) {
    throw new Error("signature is not PKCS#7 SignedData");
  }
  const signedData = children(children(contentInfo[1]!)[0]!);
  if (!signedData.some((n) => n.tag === TAG_CONTEXT && n.body.includes(cert.certDer))) {
    throw new Error("signature doesn't carry the signing certificate");
  }
  const signerInfo = children(children(signedData[signedData.length - 1]!)[0]!);
  // version, sid, digestAlgorithm, [0] signedAttrs?, signatureAlgorithm, signature
  const digestName = DIGEST_BY_OID.get(children(signerInfo[2]!)[0]!.raw.toString("hex"));
  if (!digestName) {
    throw new Error("signature uses an unknown digest");
  }
  const sig = signerInfo.find((n) => n.tag === TAG_OCTET_STRING)!.body;
  const attrs = signerInfo.find((n) => n.tag === TAG_CONTEXT);
  if (!attrs) {
    if (!rsaVerify(digestName, signed, cert.publicKey, sig)) {
      throw new Error("signature doesn't match the document");
    }
    return;
  }

  const digestAttr = children(attrs).find((n) => children(n)[0]!.raw.equals(oid(OID_MESSAGE_DIGEST)));
  const wantDigest = digestAttr && children(children(digestAttr)[1]!)[0]!.body;
  const gotDigest = createHash(digestName).update(signed).digest();
  if (!wantDigest || !gotDigest.equals(wantDigest)) {
    throw new Error("the document changed since signing");
  }
  // signed over the attributes as a SET, not as the [0] they are stored as
  const attrsAsSet = Buffer.from(attrs.raw);
  attrsAsSet[0] = TAG_SET;
  if (!rsaVerify(digestName, attrsAsSet, cert.publicKey, sig)) {
    throw new Error("signature wasn't made with the test certificate");
  }
}
