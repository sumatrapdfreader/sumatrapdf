// Generates chain-signed.ofd, an OFD with an SES v4 seal whose signer certificate is issued
// by a test root, and chain-root.pem, that root. Keys are made fresh and thrown away.
// Needs OpenSSL 3 (SM2, SM3). Run: bun tests/issue-1909-data/make-signed.ts

import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { makePng, makeZip } from "../util.ts";

const ns = 'xmlns:ofd="http://www.ofdspec.org/2016"';
const distid = "distid:1234567812345678";
const work = mkdtempSync(join(tmpdir(), "ofd-signed-"));
const w = (name: string) => join(work, name);

function openssl(args: string[], input?: Buffer): Buffer {
  const res = spawnSync("openssl", args, { input });
  if (res.status !== 0) throw new Error(`openssl ${args.join(" ")}: ${res.stderr}`);
  return res.stdout;
}

// test root and the signer it issues, SM2 with the standard user ID
for (const name of ["root", "signer"]) {
  openssl(["genpkey", "-algorithm", "EC", "-pkeyopt", "ec_paramgen_curve:SM2", "-out", w(`${name}.key`)]);
}
openssl(
  [
    "req",
    "-new",
    "-x509",
    "-key",
    w("root.key"),
    "-sm3",
    "-sigopt",
    distid,
    "-subj",
    "/CN=SumatraPDF Test Root",
  ].concat(["-days", "36500", "-out", w("root.pem")]),
);
openssl(
  ["req", "-new", "-key", w("signer.key"), "-sm3", "-sigopt", distid, "-subj", "/CN=SumatraPDF Test Signer"].concat([
    "-out",
    w("signer.csr"),
  ]),
);
openssl(
  ["x509", "-req", "-in", w("signer.csr"), "-CA", w("root.pem"), "-CAkey", w("root.key"), "-sm3"].concat([
    "-sigopt",
    distid,
    "-vfyopt",
    distid,
    "-days",
    "36500",
    "-set_serial",
    "2",
    "-out",
    w("signer.pem"),
  ]),
);
const signerDer = openssl(["x509", "-in", w("signer.pem"), "-outform", "DER"]);

const sm3 = (data: Buffer) => openssl(["dgst", "-sm3", "-binary"], data);
function sign(key: string, data: Buffer): Buffer {
  writeFileSync(w("tbs.der"), data);
  openssl(
    ["pkeyutl", "-sign", "-inkey", w(key), "-rawin", "-in", w("tbs.der"), "-digest", "sm3"].concat([
      "-pkeyopt",
      distid,
      "-out",
      w("sig.der"),
    ]),
  );
  return readFileSync(w("sig.der"));
}

// DER
function der(tag: number, content: Buffer): Buffer {
  const n = content.length;
  const len =
    n < 128
      ? [n]
      : n < 256
        ? [0x81, n]
        : n < 65536
          ? [0x82, n >> 8, n & 255]
          : [0x83, n >> 16, (n >> 8) & 255, n & 255];
  return Buffer.concat([Buffer.from([tag, ...len]), content]);
}
const seq = (...parts: Buffer[]) => der(0x30, Buffer.concat(parts));
const int = (n: number) => der(0x02, Buffer.from([n]));
const ia5 = (s: string) => der(0x16, Buffer.from(s));
const utf8 = (s: string) => der(0x0c, Buffer.from(s));
const octets = (b: Buffer) => der(0x04, b);
const bits = (b: Buffer) => der(0x03, Buffer.concat([Buffer.from([0]), b]));
const time = (s: string) => der(0x18, Buffer.from(s));
const sm2WithSm3 = der(0x06, Buffer.from([0x2a, 0x81, 0x1c, 0xcf, 0x55, 0x01, 0x83, 0x75]));

const page =
  `<ofd:Page ${ns}><ofd:Content><ofd:Layer><ofd:PathObject ID="5" Boundary="10 10 60 40" Fill="true" Stroke="false">` +
  `<ofd:FillColor Value="0 0 255"/><ofd:AbbreviatedData>M 0 0 L 60 0 L 60 40 L 0 40 C</ofd:AbbreviatedData>` +
  `</ofd:PathObject></ofd:Layer></ofd:Content></ofd:Page>`;
const files = new Map<string, Buffer>([
  [
    "OFD.xml",
    Buffer.from(
      `<ofd:OFD ${ns}><ofd:DocBody><ofd:DocRoot>Doc_0/Document.xml</ofd:DocRoot>` +
        `<ofd:Signatures>Doc_0/Signs/Signatures.xml</ofd:Signatures></ofd:DocBody></ofd:OFD>`,
    ),
  ],
  [
    "Doc_0/Document.xml",
    Buffer.from(
      `<ofd:Document ${ns}><ofd:CommonData><ofd:PageArea><ofd:PhysicalBox>0 0 80 60</ofd:PhysicalBox></ofd:PageArea>` +
        `</ofd:CommonData><ofd:Pages><ofd:Page ID="3" BaseLoc="Pages/Page_0/Content.xml"/></ofd:Pages></ofd:Document>`,
    ),
  ],
  ["Doc_0/Pages/Page_0/Content.xml", Buffer.from(page)],
  [
    "Doc_0/Signs/Signatures.xml",
    Buffer.from(
      `<ofd:Signatures ${ns}><ofd:Signature ID="1" Type="Seal" BaseLoc="Sign_0/Signature.xml"/></ofd:Signatures>`,
    ),
  ],
]);

const references = ["OFD.xml", "Doc_0/Document.xml", "Doc_0/Pages/Page_0/Content.xml"]
  .map(
    (f) =>
      `<ofd:Reference FileRef="/${f}"><ofd:CheckValue>${sm3(files.get(f)!).toString("base64")}</ofd:CheckValue></ofd:Reference>`,
  )
  .join("");
const signatureXml = Buffer.from(
  `<ofd:Signature ${ns}><ofd:SignedInfo><ofd:Provider ProviderName="SumatraPDF test"/>` +
    `<ofd:SignatureMethod>1.2.156.10197.1.501</ofd:SignatureMethod>` +
    `<ofd:References CheckMethod="1.2.156.10197.1.401">${references}</ofd:References>` +
    `<ofd:StampAnnot ID="1" PageRef="3" Boundary="45 25 20 20"/></ofd:SignedInfo>` +
    `<ofd:SignedValue>/Doc_0/Signs/Sign_0/SignedValue.dat</ofd:SignedValue></ofd:Signature>`,
);
files.set("Doc_0/Signs/Sign_0/Signature.xml", signatureXml);

// SES v4 (GM/T 0031): the seal, then TBS_Sign signed by the signer
// GeneralizedTime of now: the certificates were just made, so the signature is within their validity
const now = new Date().toISOString().replace(/[-:T]/g, "").slice(0, 14) + "Z";
const sealInfo = seq(
  seq(ia5("ES"), int(4), ia5("SumatraPDF")),
  ia5("test-seal-1"),
  seq(int(1), utf8("SumatraPDF test seal"), int(1), seq(), time(now), time(now), time("20991231000000Z")),
  seq(ia5("PNG"), octets(makePng(4, 4, [255, 0, 0])), int(20), int(20)),
);
const seal = seq(sealInfo, seq(octets(signerDer), sm2WithSm3, bits(sign("root.key", sealInfo))));
const tbsSign = seq(int(4), seal, time(now), bits(sm3(signatureXml)), ia5("/Doc_0/Signs/Sign_0/Signature.xml"));
const sesSignature = seq(tbsSign, octets(signerDer), sm2WithSm3, bits(sign("signer.key", tbsSign)));
files.set("Doc_0/Signs/Sign_0/SignedValue.dat", sesSignature);

writeFileSync(join(import.meta.dir, "chain-signed.ofd"), makeZip([...files].map(([name, data]) => ({ name, data }))));
writeFileSync(join(import.meta.dir, "chain-root.pem"), readFileSync(w("root.pem")));
rmSync(work, { recursive: true, force: true });
console.log("wrote chain-signed.ofd and chain-root.pem");
