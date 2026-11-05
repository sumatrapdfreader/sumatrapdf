// Synthetic signature appearances need neither certificates nor private invoice data.
// Run: bun tests/ofd-seals.ts [--no-build]

import { writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, loadPng, makePng, makeZip, pngChunk, pngPixel, runStandalone, tmpPath } from "./util.ts";

const ns = 'xmlns="http://www.ofdspec.org/2016"';
const dpi = 254;
const pixelsPerMm = 10;
const derSequence = 0x30;
const derInteger = 0x02;
const derOctetString = 0x04;
const derIA5String = 0x16;

function der(tag: number, content: Buffer): Buffer {
  const bytes: number[] = [];
  for (let n = content.length; n; n >>>= 8) bytes.unshift(n & 255);
  const length = content.length < 128 ? [content.length] : [128 | bytes.length, ...bytes];
  return Buffer.concat([Buffer.from([tag, ...length]), content]);
}

const seq = (...parts: Buffer[]) => der(derSequence, Buffer.concat(parts));
const integer = (n: number) => der(derInteger, Buffer.from([n]));
const string = (s: string) => der(derIA5String, Buffer.from(s));

function pack(files: [string, string | Buffer][]): Buffer {
  return makeZip(files.map(([name, content]) => ({ name, data: Buffer.from(content) })));
}

function sealOfd(): Buffer {
  return pack([
    ["OFD.xml", `<OFD ${ns}><DocBody><DocRoot>Doc/main.xml</DocRoot></DocBody></OFD>`],
    [
      "Doc/main.xml",
      `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 210 297</PhysicalBox></PageArea></CommonData>` +
        `<Pages><Page ID="1" BaseLoc="page.xml"/></Pages></Document>`,
    ],
    [
      "Doc/page.xml",
      `<Page ${ns}><Area><PhysicalBox>0 0 104 84</PhysicalBox></Area><Content><Layer>` +
        `<PathObject ID="2" Boundary="0 0 104 84" Fill="true" Stroke="false"><FillColor Value="255 0 0"/>` +
        `<AbbreviatedData>M 0 0 L 104 0 L 104 84 L 0 84 C</AbbreviatedData></PathObject></Layer></Content></Page>`,
    ],
  ]);
}

function opaqueSeal(): Buffer {
  const size = 8;
  const channels = 3;
  const stride = 1 + size * channels;
  const rows = Buffer.alloc(size * stride, 255);
  for (let y = 0; y < size; ++y) {
    rows[y * stride] = 0;
    if (y < 2 || y >= 6) continue;
    for (let x = 2; x < 6; ++x) {
      rows[y * stride + 1 + x * channels + 1] = 0;
      rows[y * stride + 1 + x * channels + 2] = 0;
    }
  }
  const header = Buffer.alloc(13);
  header.writeUInt32BE(size, 0);
  header.writeUInt32BE(size, 4);
  header[8] = 8;
  header[9] = 2;
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", header),
    pngChunk("IDAT", deflateSync(rows)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function fixture(): Buffer {
  const picture = (type: string, data: Buffer) =>
    seq(string(type), der(derOctetString, data), integer(34), integer(26));
  const signed = (image: Buffer) =>
    seq(seq(integer(4), seq(seq(seq(string("ES"), integer(4), string("TEST")), string("test-seal"), seq(), image))));
  const signature = (page: number, boundary: string, path: string) =>
    `<Signature ${ns}><SignedInfo><StampAnnot ID="1" PageRef="${page}" Boundary="${boundary}"` +
    (path.endsWith("one.dat") ? ` Clip="0 0 6 8"` : "") +
    ` /></SignedInfo>` +
    `<SignedValue>${path}</SignedValue></Signature>`;
  return pack([
    [
      "OFD.xml",
      `<OFD ${ns}><DocBody><DocRoot>Doc/main.xml</DocRoot><Signatures>/Doc/Signs/index.xml</Signatures></DocBody></OFD>`,
    ],
    [
      "Doc/main.xml",
      `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 40 30</PhysicalBox></PageArea></CommonData>` +
        `<Pages><Page ID="11" BaseLoc="page.xml"/><Page ID="22" BaseLoc="page.xml"/></Pages></Document>`,
    ],
    [
      "Doc/page.xml",
      `<Page ${ns}><Content><Layer>` +
        `<PathObject ID="3" Boundary="14 20 12 8" Fill="true" Stroke="false"><FillColor Value="128 192 255"/>` +
        `<AbbreviatedData>M 0 0 L 12 0 L 12 8 L 0 8 C</AbbreviatedData></PathObject>` +
        `<PathObject ID="4" Boundary="14 21.5 12 0.4" LineWidth="0.4"><AbbreviatedData>M 0 0 L 12 0</AbbreviatedData></PathObject>` +
        `</Layer></Content></Page>`,
    ],
    [
      "Doc/Signs/index.xml",
      `<Signatures ${ns}><Signature ID="1" BaseLoc="one.xml"/>` +
        `<Signature ID="2" BaseLoc="/Doc/Signs/two.xml"/><Signature ID="3" BaseLoc="broken.xml"/>` +
        `<Signature ID="4" BaseLoc="opaque.xml"/></Signatures>`,
    ],
    ["Doc/Signs/one.xml", signature(22, "8 6 12 8", "/Doc/Signs/one.dat")],
    ["Doc/Signs/one.dat", signed(picture("ofd", sealOfd()))],
    ["Doc/Signs/two.xml", signature(22, "24 6 6 12", "two.dat")],
    ["Doc/Signs/two.dat", signed(picture("png", makePng(4, 2, [0, 0, 255])))],
    ["Doc/Signs/broken.xml", signature(22, "4 20 8 4", "broken.dat")],
    ["Doc/Signs/broken.dat", Buffer.from([derSequence, 0x88, 255, 255, 255, 255, 255, 255, 255, 255])],
    ["Doc/Signs/opaque.xml", signature(22, "14 20 12 8", "opaque.dat")],
    ["Doc/Signs/opaque.dat", signed(picture("png", opaqueSeal()))],
  ]);
}

export async function testit(): Promise<void> {
  const file = tmpPath("ofd-seals.ofd");
  const template = tmpPath("ofd-seals-<N>.png");
  writeFileSync(file, fixture());
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const result = await client.request(ControlCommand.TestConvertToImages, [template, "all", dpi]);
      if (result[0] !== 0 || !String(result[1]).startsWith("OK")) throw new Error(`OFD seal export failed: ${result}`);
    },
    [file],
  );

  const first = loadPng(tmpPath("ofd-seals-1.png"));
  const second = loadPng(tmpPath("ofd-seals-2.png"));
  const check = (image: typeof first, x: number, y: number, expected: number[]) => {
    const actual = pngPixel(image, Math.round(x * pixelsPerMm), Math.round(y * pixelsPerMm));
    if (actual.some((n, i) => Math.abs(n - expected[i]) > 10))
      throw new Error(`OFD seal at ${x},${y}: expected ${expected}, got ${actual}`);
  };
  check(first, 14, 10, [255, 255, 255]);
  check(first, 15, 20.5, [128, 192, 255]);
  check(second, 15, 20.5, [128, 192, 255]);
  check(second, 16, 21.5, [0, 0, 0]);
  check(second, 20, 24, [128, 0, 0]);
  check(second, 8, 22, [255, 255, 255]);
  for (const [x, y] of [
    [8.5, 6.5],
    [13.5, 13.5],
    [10, 10],
  ])
    check(second, x, y, [255, 0, 0]);
  for (const [x, y] of [
    [24.5, 6.5],
    [29.5, 17.5],
  ])
    check(second, x, y, [0, 0, 255]);
  for (const [x, y] of [
    [7, 10],
    [14.5, 10],
    [21, 10],
    [14, 15],
    [31, 10],
    [27, 19],
  ])
    check(second, x, y, [255, 255, 255]);
}

if (import.meta.main) {
  await runStandalone(testit);
}
