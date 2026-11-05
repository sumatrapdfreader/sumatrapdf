// Generates encrypted.ofd: a 2-page OFD encrypted with the password "sumatra" the way
// ofdrw's OFDEncryptor does it (GM/T 0099). Needs OpenSSL 3 for SM3 and SM4.
// Run: bun tests/issue-1909-data/make-encrypted.ts

import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { makeZip } from "../util.ts";

const password = "sumatra";
const ns = 'xmlns:ofd="http://www.ofdspec.org/2016"';

function openssl(args: string[], input: Buffer): Buffer {
  const res = spawnSync("openssl", args, { input });
  if (res.status !== 0) throw new Error(`openssl ${args.join(" ")}: ${res.stderr}`);
  return res.stdout;
}

const sm3 = (data: Buffer) => openssl(["dgst", "-sm3", "-binary"], data);
const sm4 = (data: Buffer, key: Buffer, iv: Buffer) =>
  openssl(["enc", "-sm4-cbc", "-K", key.toString("hex"), "-iv", iv.toString("hex")], data);

// GB/T 32918.3 KDF with SM3, one block is enough for a 16-byte key
const kek = sm3(Buffer.concat([Buffer.from(password), Buffer.from([0, 0, 0, 1])])).subarray(0, 16);

const page = (color: string) =>
  `<ofd:Page ${ns}><ofd:Content><ofd:Layer><ofd:PathObject ID="2" Boundary="10 10 30 30" Fill="true" Stroke="false">` +
  `<ofd:FillColor Value="${color}"/><ofd:AbbreviatedData>M 0 0 L 30 0 L 30 30 L 0 30 C</ofd:AbbreviatedData>` +
  `</ofd:PathObject></ofd:Layer></ofd:Content></ofd:Page>`;
const plain: [string, string][] = [
  ["/OFD.xml", `<ofd:OFD ${ns}><ofd:DocBody><ofd:DocRoot>Doc_0/Document.xml</ofd:DocRoot></ofd:DocBody></ofd:OFD>`],
  [
    "/Doc_0/Document.xml",
    `<ofd:Document ${ns}><ofd:CommonData><ofd:PageArea><ofd:PhysicalBox>0 0 50 50</ofd:PhysicalBox></ofd:PageArea>` +
      `</ofd:CommonData><ofd:Pages><ofd:Page ID="3" BaseLoc="Pages/Page_0/Content.xml"/>` +
      `<ofd:Page ID="4" BaseLoc="Pages/Page_1/Content.xml"/></ofd:Pages></ofd:Document>`,
  ],
  ["/Doc_0/Pages/Page_0/Content.xml", page("255 0 0")],
  ["/Doc_0/Pages/Page_1/Content.xml", page("0 0 255")],
];

const fek = randomBytes(16);
const iv = randomBytes(16);
const entries: { name: string; data: Buffer }[] = [];
const map: string[] = [];
for (const [path, xml] of plain) {
  const epath = path.replace(/\.xml$/, ".dat");
  entries.push({ name: epath.slice(1), data: sm4(Buffer.from(xml), fek, iv) });
  map.push(`<ofd:EncryptEntry Path="${path}" EPath="${epath}"/>`);
}
const entriesMap = `<ofd:EncryptEntries ${ns} ID="1">${map.join("")}</ofd:EncryptEntries>`;
entries.push({ name: "entriesmap.dat", data: sm4(Buffer.from(entriesMap), fek, iv) });
entries.push({
  name: "decryptseed.dat",
  data: Buffer.from(
    `<ofd:DecyptSeed ${ns} ID="1"><ofd:UserInfo UserName="user" UserType="Owner">` +
      `<ofd:EncryptedWK>${sm4(fek, kek, iv).toString("base64")}</ofd:EncryptedWK>` +
      `<ofd:IVValue>${iv.toString("base64")}</ofd:IVValue></ofd:UserInfo></ofd:DecyptSeed>`,
  ),
});
entries.push({
  name: "Encryptions.xml",
  data: Buffer.from(
    `<ofd:Encryptions ${ns}><ofd:EncryptInfo ID="1"><ofd:EncryptScope>All</ofd:EncryptScope>` +
      `<ofd:DecryptSeedLoc>/decryptseed.dat</ofd:DecryptSeedLoc><ofd:EntriesMapLoc>/entriesmap.dat</ofd:EntriesMapLoc>` +
      `</ofd:EncryptInfo></ofd:Encryptions>`,
  ),
});

const out = join(import.meta.dir, "encrypted.ofd");
writeFileSync(out, makeZip(entries));
console.log(`wrote ${out}`);
