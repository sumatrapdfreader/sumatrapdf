// Native OFD documents (#1909): templates, inherited paint, text, paths, seals, the features of
// the OFDRW samples, packages, encryption, links, attachments, conversion and signature checks.
// Run: bun tests/issue-1909.ts [--no-build]

import { copyFileSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { deflateSync } from "node:zlib";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import {
  assemblePdf,
  EXE,
  extractPageText,
  loadPng,
  makePng,
  makeZip,
  pngChunk,
  pngPixel,
  pollUntil,
  readZip,
  ROOT,
  runStandalone,
  SLOW_BUILD_FACTOR,
  tmpPath,
  USE_NG,
  writeAppdata,
} from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const ns = 'xmlns="http://www.ofdspec.org/2016"';

// --- basic: templates, inherited paint, annotation images, renamed .zip

function makeOfd(): Buffer {
  const ns = 'xmlns:o="http://www.ofdspec.org/2016"';
  const xml = (tag: string, body: string) => `<o:${tag} ${ns}>${body}</o:${tag}>`;
  const text = (value: string, y: number) =>
    `<o:TextObject Boundary="10 ${y} 80 8" Font="5" Size="5">` +
    `<o:TextCode X="0" Y="6" DeltaX="g ${value.length - 1} 3">${value}</o:TextCode></o:TextObject>`;
  const path = (size: number) =>
    `<o:PathObject Boundary="10 10 ${size} ${size}" Fill="true" Stroke="false">` +
    `<o:AbbreviatedData>M 0 0 L ${size} 0 L ${size} ${size} L 0 ${size} C</o:AbbreviatedData></o:PathObject>`;
  const entries = [
    ["OFD.xml", xml("OFD", "<o:DocBody><o:DocRoot>Docs/main.xml</o:DocRoot></o:DocBody>")],
    [
      "Docs/main.xml",
      xml(
        "Document",
        `<o:CommonData><o:PageArea><o:PhysicalBox>0 0 100 80</o:PhysicalBox></o:PageArea>` +
          `<o:PublicRes>Resources.xml</o:PublicRes><o:TemplatePage ID="1" BaseLoc="Tpl/page.xml"/></o:CommonData>` +
          `<o:Pages><o:Page ID="11" BaseLoc="Pages/one.xml"/><o:Page ID="12" BaseLoc="Pages/two.xml"/></o:Pages>` +
          `<o:Annotations>Annots/index.xml</o:Annotations>`,
      ),
    ],
    [
      "Docs/Resources.xml",
      `<o:Res ${ns} BaseLoc="Assets"><o:Fonts><o:Font ID="5" FontName="Arial"/></o:Fonts>` +
        `<o:DrawParams><o:DrawParam ID="6"><o:FillColor Value="255 0 0"/></o:DrawParam>` +
        `<o:DrawParam ID="7" Relative="6"/><o:DrawParam ID="8"><o:FillColor Value="0 0 255"/></o:DrawParam></o:DrawParams>` +
        `<o:MultiMedias><o:MultiMedia ID="9" Type="Image"><o:MediaFile>blue.png</o:MediaFile></o:MultiMedia></o:MultiMedias></o:Res>`,
    ],
    [
      "Docs/Tpl/page.xml",
      xml(
        "Page",
        `<o:Content><o:Layer DrawParam="8">${path(20)}</o:Layer><o:Layer>${text("TEMPLATE", 55)}</o:Layer></o:Content>`,
      ),
    ],
    ...["ALPHA", "BETA"].map((label, i) => [
      `Docs/Pages/${i === 0 ? "one" : "two"}.xml`,
      xml(
        "Page",
        (i ? "<o:Area><o:PhysicalBox>0 0 120 90</o:PhysicalBox></o:Area>" : "") +
          `<o:Template TemplateID="1" ZOrder="Background"/><o:Content><o:Layer DrawParam="7">${path(10)}</o:Layer>` +
          `<o:Layer>${text(label, 40)}</o:Layer></o:Content>`,
      ),
    ]),
    ["Docs/Annots/index.xml", xml("Annotations", `<o:Page PageID="11"><o:FileLoc>one.xml</o:FileLoc></o:Page>`)],
    [
      "Docs/Annots/one.xml",
      xml(
        "PageAnnot",
        `<o:Annot ID="10" Type="Watermark"><o:Appearance Boundary="40 10 10 10">` +
          `<o:ImageObject Boundary="0 0 10 10" CTM="10 0 0 10 0 0" ResourceID="9"/></o:Appearance></o:Annot>`,
      ),
    ],
  ];
  return makeZip([
    ...entries.map(([name, content]) => ({ name, data: Buffer.from(content) })),
    { name: "Docs/Assets/blue.png", data: makePng(2, 2, [0, 0, 255]) },
  ]);
}

async function testBasic(): Promise<void> {
  const file = tmpPath("native-ofd.ofd");
  writeFileSync(file, makeOfd());
  for (const [page, label] of [
    [1, "ALPHA"],
    [2, "BETA"],
  ] as const) {
    const text = extractPageText(file, page).replace(/\s/g, "");
    if (!text.includes(label) || !text.includes("TEMPLATE")) {
      throw new Error(`OFD page ${page}: missing page/template text: ${JSON.stringify(text)}`);
    }
  }

  const renamed = tmpPath("native-ofd.zip");
  copyFileSync(file, renamed);
  await withControlledSumatra(
    EXE,
    async (client) => {
      for (const [path, page] of [
        [file, 1],
        [file, 2],
        [renamed, 1],
      ] as const) {
        const result = await client.request(ControlCommand.TestRenderPageColors, [path, page, 100]);
        const raw = String(result[1] ?? "");
        console.log(`OFD page ${page}: ${raw.trim()}`);
        if (result[0] !== 0 || !raw.includes("pages=2")) {
          throw new Error(`OFD render failed: ${raw}`);
        }
        const red = Number(/red=(\d+)/.exec(raw)?.[1]);
        const blue = Number(/blue=(\d+)/.exec(raw)?.[1]);
        if (!(red > 600 && red < 1000 && blue > 2000 && blue < 3600)) {
          throw new Error(`OFD template order, inherited color or image failed: ${raw}`);
        }
        if (!raw.includes(page === 1 ? "size=284x227" : "size=341x256")) {
          throw new Error(`OFD page dimensions failed: ${raw}`);
        }
      }
    },
    ["-log-to-file", tmpPath("ofd.log")],
  );
}

// --- path fill defaults to transparent; text fill defaults to black

const pathFillDpi = 720;
const mmPerInch = 25.4;

function makeSymbol(): Buffer {
  const files = [
    ["OFD.xml", `<OFD ${ns}><DocBody><DocRoot>Doc/Document.xml</DocRoot></DocBody></OFD>`],
    [
      "Doc/Document.xml",
      `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 25 25</PhysicalBox></PageArea></CommonData>` +
        `<Pages><Page ID="1" BaseLoc="Page.xml"/></Pages></Document>`,
    ],
    [
      "Doc/Page.xml",
      `<Page ${ns}><Content><Layer><PathObject ID="2" Boundary="10 10 5 5" LineWidth="0.225" Fill="true">` +
        `<AbbreviatedData>M 4.5315 2.493 B 4.5315 1.368 3.618 0.45 2.4885 0.45 ` +
        `B 1.3635 0.45 0.45 1.368 0.45 2.493 B 0.45 3.618 1.3635 4.536 2.4885 4.536 ` +
        `B 3.618 4.536 4.5315 3.618 4.5315 2.493 M 1.035 1.035 L 3.915 3.915 ` +
        `M 1.035 3.915 L 3.915 1.035</AbbreviatedData></PathObject></Layer></Content></Page>`,
    ],
  ];
  return makeZip(files.map(([name, xml]) => ({ name, data: Buffer.from(xml) })));
}

async function testPathFill(): Promise<void> {
  const file = tmpPath("ofd-path-fill.ofd");
  const output = tmpPath("ofd-path-fill-1.png");
  writeFileSync(file, makeSymbol());
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const result = await client.request(ControlCommand.TestConvertToImages, [output, "1", pathFillDpi]);
      if (result[0] !== 0 || !String(result[1]).startsWith("OK")) {
        throw new Error(`OFD symbol export failed: ${result}`);
      }
    },
    [file],
  );

  const image = loadPng(output);
  const pixel = (x: number, y: number) =>
    pngPixel(image, Math.round((x * pathFillDpi) / mmPerInch), Math.round((y * pathFillDpi) / mmPerInch));
  for (const [x, y] of [
    [11.5, 12.49],
    [13.5, 12.49],
    [12.49, 11.5],
    [12.49, 13.5],
  ]) {
    const rgb = pixel(x, y);
    if (Math.min(...rgb) < 245) {
      throw new Error(`OFD transparent path fill: expected white at ${x},${y}, got ${rgb}`);
    }
  }
  for (const [x, y] of [
    [10.45, 12.49],
    [12.49, 12.49],
    [11.7, 11.7],
    [11.7, 13.25],
  ]) {
    const rgb = pixel(x, y);
    if (Math.max(...rgb) > 40) {
      throw new Error(`OFD circle/cross stroke: expected black at ${x},${y}, got ${rgb}`);
    }
  }
}

// --- synthetic signature appearances need neither certificates nor private invoice data

const sealDpi = 254;
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

function sealFixture(): Buffer {
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

async function testSeals(): Promise<void> {
  const file = tmpPath("ofd-seals.ofd");
  const template = tmpPath("ofd-seals-<N>.png");
  writeFileSync(file, sealFixture());
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const result = await client.request(ControlCommand.TestConvertToImages, [template, "all", sealDpi]);
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

// --- focused fixtures for the OFDRW rendering cases

const rectangle = (w: number, h: number) => `M 0 0 L ${w} 0 L ${w} ${h} L 0 ${h} C`;
const path = (data: string, attrs: string, fill: string, clips = "") =>
  `<PathObject ID="3" Boundary="4 4 20 20" Fill="true" Stroke="false" ${attrs}>` +
  `<FillColor>${fill}</FillColor>${clips}<AbbreviatedData>${data}</AbbreviatedData></PathObject>`;
const red =
  `<PathObject ID="4" Boundary="0 0 20 20" Fill="true" Stroke="false">` +
  `<FillColor Value="255 0 0"/><AbbreviatedData>${rectangle(20, 20)}</AbbreviatedData></PathObject>`;
const redPath = (data: string, fill: string) =>
  `<PathObject ID="5" Boundary="4 4 20 20" Fill="true" Stroke="false">${fill}<AbbreviatedData>${data}</AbbreviatedData></PathObject>`;
const meshPoint = (x: number, y: number, color: string, edgeFlag?: number) =>
  `<Point X="${x}" Y="${y}"${edgeFlag === undefined ? "" : ` EdgeFlag="${edgeFlag}"`}><Color Value="${color}"/></Point>`;
const redUnit = `<CompositeGraphicUnits><CompositeGraphicUnit ID="10" Width="20" Height="20"><Content>${red}</Content></CompositeGraphicUnit></CompositeGraphicUnits>`;

type Counts = { red: number; blue: number; nonwhite: number; right: number };
type Case = {
  name: string;
  body: string;
  resources?: string;
  common?: string;
  font?: Buffer;
  base?: string;
  image?: Buffer;
  text?: string;
  // inserted into the page before <Content>, e.g. <Template> or <PageRes>
  pageHead?: string;
  // inserted into Document.xml after <Pages> and into OFD.xml's <DocBody>
  docTail?: string;
  docBody?: string;
  files?: [string, string][];
  check: (c: Counts) => boolean;
};

const axial = `<AxialShd StartPoint="0 0" EndPoint="20 0" Extend="3"><Segment Position="0"><Color Value="255 0 0"/></Segment><Segment Position="1"><Color Value="0 0 255"/></Segment></AxialShd>`;

const cases: Case[] = [
  ...(IS_WIN
    ? [
        {
          name: "system-ligature",
          resources: `<Fonts><Font ID="5" FontName="Times New Roman"/></Fonts>`,
          body:
            `<TextObject ID="6" Boundary="4 4 25 10" Font="5" Size="8"><CGTransform CodePosition="0" CodeCount="2" GlyphCount="1">` +
            `<Glyphs>191</Glyphs></CGTransform><TextCode X="0" Y="8" DeltaX="8">fiX</TextCode></TextObject>`,
          text: "fiX",
          check: (c: Counts) => c.right > 35 && c.right < 55,
        },
      ]
    : []),
  {
    name: "ligature-position",
    font: readFileSync(join(ROOT, "tests/issue-3219-font.cff")),
    resources: `<Fonts><Font ID="5" FontName="Subset"><FontFile>font.cff</FontFile></Font></Fonts>`,
    body:
      `<TextObject ID="6" Boundary="4 4 25 10" Font="5" Size="8"><CGTransform CodePosition="0" CodeCount="2" GlyphCount="1">` +
      `<Glyphs>34</Glyphs></CGTransform><TextCode X="0" Y="8" DeltaX="8">\nABX\n</TextCode></TextObject>`,
    text: "ABX",
    check: (c) => c.right > 35 && c.right < 55,
  },
  {
    name: "expanded-position",
    font: readFileSync(join(ROOT, "tests/issue-3219-font.cff")),
    resources: `<Fonts><Font ID="5" FontName="Subset"><FontFile>font.cff</FontFile></Font></Fonts>`,
    body:
      `<TextObject ID="6" Boundary="4 4 25 10" Font="5" Size="8"><CGTransform CodePosition="0" CodeCount="1" GlyphCount="2">` +
      `<Glyphs>34 34</Glyphs></CGTransform><TextCode X="0" Y="8" DeltaX="8 8">AX</TextCode></TextObject>`,
    text: "AX",
    check: (c) => c.right > 65 && c.right < 80,
  },
  {
    name: "recovered-image",
    base: "wrong-directory",
    image: makePng(2, 2, [0, 0, 255]),
    resources: `<MultiMedias><MultiMedia ID="9" Type="Image"><MediaFile>blue.png</MediaFile></MultiMedia></MultiMedias>`,
    body: `<ImageObject ID="1" Boundary="4 4 10 10" CTM="10 0 0 10 0 0" ResourceID="9"/>`,
    check: (c) => c.blue > 700,
  },
  {
    name: "dash",
    body:
      `<PathObject ID="1" Boundary="4 4 20 2" LineWidth="1" DashPattern="1 1">` +
      `<StrokeColor Value="255 0 0"/><AbbreviatedData>M 0 1 L 20 1</AbbreviatedData></PathObject>`,
    check: (c) => c.red > 30 && c.red < 100,
  },
  {
    name: "missing",
    body: red + `<ImageObject ID="8" Boundary="0 0 1 1" ResourceID="999"/>`,
    common: `<PublicRes>missing.xml</PublicRes>`,
    check: (c) => c.red > 3000,
  },
  {
    name: "composite",
    resources: redUnit,
    body: `<CompositeObject ID="1" ResourceID="10" Boundary="4 4 10 10" CTM="0.5 0 0 0.5 0 0" Alpha="127"/>`,
    check: (c) => c.red === 0 && c.nonwhite > 700 && c.nonwhite < 1000,
  },
  {
    name: "clip",
    body: path(
      rectangle(10, 10),
      `CTM="2 0 0 2 0 0"`,
      `<ColorPlaceholder/>`,
      `<Clips TransFlag="false"><Clip><Area><Path Fill="true" Stroke="false"><AbbreviatedData>${rectangle(4, 20)}</AbbreviatedData></Path></Area>` +
        `<Area CTM="1 0 0 1 6 0"><Path Fill="true" Stroke="false"><AbbreviatedData>${rectangle(4, 20)}</AbbreviatedData></Path></Area></Clip>` +
        `<Clip><Area><Path Fill="true" Stroke="false"><AbbreviatedData>${rectangle(10, 10)}</AbbreviatedData></Path></Area></Clip></Clips>`,
    ).replace(`<FillColor><ColorPlaceholder/></FillColor>`, `<FillColor Value="255 0 0"/>`),
    check: (c) => c.red > 500 && c.red < 800,
  },
  {
    name: "pattern",
    body: path(
      rectangle(20, 20),
      "",
      `<Pattern Width="2" Height="2" XStep="4" YStep="4">` +
        `<CellContent><PathObject ID="4" Boundary="0 0 2 2" Fill="true" Stroke="false"><FillColor Value="255 0 0"/>` +
        `<AbbreviatedData>${rectangle(2, 2)}</AbbreviatedData></PathObject></CellContent></Pattern>`,
    ),
    check: (c) => c.red > 600 && c.red < 1000,
  },
  ...[false, true].map((stroke): Case => ({
    name: stroke ? "gradient-text-stroke" : "gradient-text-fill",
    resources: `<Fonts><Font ID="5" FontName="Arial"/></Fonts>`,
    body: `<TextObject ID="6" Boundary="4 4 22 16" Font="5" Size="10" Fill="${!stroke}" Stroke="${stroke}" LineWidth="0.5"><FillColor>${axial}</FillColor><StrokeColor>${axial}</StrokeColor><TextCode X="0" Y="12" DeltaX="7 7">HHH</TextCode></TextObject>`,
    check: (c) => c.red > 5 && c.blue > 5,
  })),
  {
    name: "gradient-path-stroke",
    body: `<PathObject ID="1" Boundary="4 4 20 4" LineWidth="2"><StrokeColor>${axial}</StrokeColor><AbbreviatedData>M 0 2 L 20 2</AbbreviatedData></PathObject>`,
    check: (c) => c.red > 10 && c.blue > 10,
  },
  {
    name: "gradient",
    body: path(
      rectangle(20, 20),
      "",
      `<RadialShd StartPoint="10 10" EndPoint="10 10" EndRadius="10" Extend="3">` +
        `<Segment Position="0"><Color Value="255 0 0"/></Segment><Segment Position="1"><Color Value="0 0 255"/></Segment></RadialShd>`,
    ),
    check: (c) => c.red > 50 && c.blue > 200 && c.nonwhite > 3000,
  },
  {
    name: "mapped",
    font: readFileSync(join(ROOT, "tests/issue-3219-font.cff")),
    resources: `<Fonts><Font ID="5" FontName="Subset"><FontFile>font.cff</FontFile></Font></Fonts>`,
    body:
      `<TextObject ID="6" Boundary="4 4 20 10" Font="5" Size="8"><CGTransform CodePosition="0" CodeCount="2" GlyphCount="1">` +
      `<Glyphs>34</Glyphs></CGTransform><TextCode X="0" Y="8" DeltaX="4">AB</TextCode></TextObject>`,
    text: "AB",
    check: (c) => c.nonwhite > 30,
  },
  {
    name: "nofont",
    resources: `<Fonts><Font ID="5" FontName="Arial"><FontFile>missing.ttf</FontFile></Font></Fonts>`,
    body:
      `<TextObject ID="6" Boundary="4 4 20 10" Font="5" Size="8"><CGTransform CodePosition="0" CodeCount="1" GlyphCount="1">` +
      `<Glyphs>65535</Glyphs></CGTransform><TextCode X="0" Y="8" DeltaX="5 5">ABC</TextCode></TextObject>`,
    text: "ABC",
    check: (c) => c.nonwhite > 50,
  },
  {
    // a 20 mm circle made of two arcs covers pi/4 of its 20 mm square
    name: "arc",
    body: redPath(`M 0 10 A 10 10 0 1 1 20 10 A 10 10 0 1 1 0 10 C`, `<FillColor Value="255 0 0"/>`),
    check: (c) => c.red > 2200 && c.red < 2850,
  },
  {
    name: "page-res",
    pageHead: `<PageRes>PageRes.xml</PageRes>`,
    files: [["Doc/PageRes.xml", `<Res ${ns}>${redUnit}</Res>`]],
    body: `<CompositeObject ID="1" ResourceID="10" Boundary="0 0 20 20"/>`,
    check: (c) => c.red > 3000,
  },
  {
    name: "nested-template",
    common: `<TemplatePage ID="30" BaseLoc="outer.xml"/><TemplatePage ID="31" BaseLoc="inner.xml"/>`,
    pageHead: `<Template TemplateID="30"/>`,
    files: [
      ["Doc/outer.xml", `<Page ${ns}><Template TemplateID="31"/><Content/></Page>`],
      ["Doc/inner.xml", `<Page ${ns}><Content><Layer>${red}</Layer></Content></Page>`],
    ],
    body: "",
    check: (c) => c.red > 3000,
  },
  {
    // a broken optional part must not take the page content with it
    name: "missing-parts",
    pageHead: `<Template TemplateID="99"/>`,
    docTail: `<Annotations>missing-annots.xml</Annotations>`,
    docBody: `<Signatures>missing-signs.xml</Signatures>`,
    body: red,
    check: (c) => c.red > 3000,
  },
  {
    name: "indexed-color",
    resources:
      `<ColorSpaces><ColorSpace ID="7" Type="RGB"><Palette><CV>#FF #00 #00</CV><CV>#00 #00 #FF</CV></Palette>` +
      `</ColorSpace></ColorSpaces>`,
    body: redPath(rectangle(20, 20), `<FillColor ColorSpace="7" Index="1"/>`),
    check: (c) => c.blue > 3000 && c.red === 0,
  },
  {
    // an object that can't be drawn (unknown path command) is skipped, not the page
    name: "unsupported-object",
    body: red + redPath(`M 0 0 Z 5 5`, `<FillColor Value="0 0 255"/>`),
    check: (c) => c.red > 3000 && c.blue === 0,
  },
  {
    // two Gouraud triangles over the square: red corner, blue corner
    name: "gouraud",
    body: redPath(
      rectangle(20, 20),
      `<FillColor><GouraudShd>` +
        meshPoint(0, 0, "255 0 0", 0) +
        meshPoint(20, 0, "255 0 0", 0) +
        meshPoint(0, 20, "0 0 255", 0) +
        meshPoint(20, 20, "0 0 255", 1) +
        `</GouraudShd></FillColor>`,
    ),
    check: (c) => c.red > 300 && c.blue > 300 && c.nonwhite > 3000,
  },
  {
    name: "lattice-gouraud",
    body: redPath(
      rectangle(20, 20),
      `<FillColor><LaGouraudShd VerticesPerRow="2">` +
        meshPoint(0, 0, "255 0 0") +
        meshPoint(20, 0, "0 0 255") +
        meshPoint(0, 20, "255 0 0") +
        meshPoint(20, 20, "0 0 255") +
        `</LaGouraudShd></FillColor>`,
    ),
    check: (c) => c.red > 300 && c.blue > 300 && c.nonwhite > 3000,
  },
];

function featureFixture(c: Case): Buffer {
  const files: [string, Buffer][] = [
    ["OFD.xml", Buffer.from(`<OFD ${ns}><DocBody><DocRoot>Doc/main.xml</DocRoot>${c.docBody ?? ""}</DocBody></OFD>`)],
    [
      "Doc/main.xml",
      Buffer.from(
        `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 30 30</PhysicalBox></PageArea>` +
          `${c.common ?? ""}<DocumentRes>Res.xml</DocumentRes></CommonData><Pages><Page ID="1" BaseLoc="page.xml"/></Pages>${c.docTail ?? ""}</Document>`,
      ),
    ],
    ["Doc/Res.xml", Buffer.from(`<Res ${ns} BaseLoc="${c.base ?? "."}">${c.resources ?? ""}</Res>`)],
    ["Doc/page.xml", Buffer.from(`<Page ${ns}>${c.pageHead ?? ""}<Content><Layer>${c.body}</Layer></Content></Page>`)],
    ...(c.files ?? []).map(([name, xml]): [string, Buffer] => [name, Buffer.from(xml)]),
  ];
  if (c.font) files.push(["Doc/font.cff", c.font]);
  if (c.image) files.push(["Doc/Assets/blue.png", c.image]);
  return makeZip(files.map(([name, data]) => ({ name, data })));
}

async function testFeatures(): Promise<void> {
  const failures: string[] = [];
  const files = cases.map((c) => {
    const file = tmpPath(`ofd-feature-${c.name}.ofd`);
    writeFileSync(file, featureFixture(c));
    return file;
  });
  await withControlledSumatra(EXE, async (client) => {
    for (const [i, c] of cases.entries()) {
      const result = await client.request(ControlCommand.TestRenderPageColors, [files[i], 1, 100]);
      const raw = String(result[1] ?? "");
      const count = (name: string) => Number(new RegExp(`\\b${name}=(\\d+)`).exec(raw)?.[1] ?? 0);
      const counts = {
        red: count("red"),
        blue: count("blue"),
        nonwhite: count("nonwhite"),
        right: Number(/box=\d+,\d+,(\d+),/.exec(raw)?.[1] ?? 0),
      };
      console.log(`${c.name}: ${raw.trim()}`);
      if (result[0] !== 0 || !c.check(counts)) failures.push(`${c.name}: ${raw.trim()}`);
    }
  });
  if (failures.length) throw new Error(failures.join("\n"));
  for (const [i, c] of cases.entries()) {
    if (c.text && !extractPageText(files[i]).replace(/\s/g, "").includes(c.text))
      throw new Error(`OFD ${c.name}: logical Unicode text was lost`);
  }
}

// --- a package with several documents opens all of them, one ToC entry per document

function packageFixture(): Buffer {
  const page = (label: string) =>
    `<Page ${ns}><Content><Layer><PathObject ID="2" Boundary="0 0 20 20" Fill="true" Stroke="false">` +
    `<FillColor Value="255 0 0"/><AbbreviatedData>M 0 0 L 20 0 L 20 20 L 0 20 C</AbbreviatedData></PathObject>` +
    `</Layer></Content></Page><!-- ${label} -->`;
  const doc = (pages: number, outline: string) =>
    `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 50 40</PhysicalBox></PageArea></CommonData><Pages>` +
    Array.from({ length: pages }, (_, i) => `<Page ID="${10 + i}" BaseLoc="p${i}.xml"/>`).join("") +
    `</Pages>${outline}</Document>`;
  const body = (dir: string, title: string) =>
    `<DocBody><DocInfo><Title>${title}</Title></DocInfo><DocRoot>${dir}/Document.xml</DocRoot></DocBody>`;
  const details =
    `<Outlines><OutlineElem Title="Details"><Actions><Action Event="CLICK"><Goto><Dest Type="XYZ" PageID="11"/>` +
    `</Goto></Action></Actions></OutlineElem></Outlines>`;
  const files: [string, string][] = [
    ["OFD.xml", `<OFD ${ns}>${body("Doc_0", "Invoice A")}${body("Doc_1", "Invoice B")}</OFD>`],
    ["Doc_0/Document.xml", doc(1, "")],
    ["Doc_0/p0.xml", page("A1")],
    ["Doc_1/Document.xml", doc(2, details)],
    ["Doc_1/p0.xml", page("B1")],
    ["Doc_1/p1.xml", page("B2")],
  ];
  return makeZip(files.map(([name, xml]) => ({ name, data: Buffer.from(xml) })));
}

async function testPackage(): Promise<void> {
  const file = tmpPath("ofd-package.ofd");
  writeFileSync(file, packageFixture());
  await withControlledSumatra(EXE, async (client) => {
    const render = await client.request(ControlCommand.TestRenderPageColors, [file, 3, 100]);
    if (render[0] !== 0 || !String(render[1]).includes("pages=3")) {
      throw new Error(`OFD package: expected 3 pages from 2 documents: ${render[1]}`);
    }
    const toc = String((await client.request(ControlCommand.TestGetToc, [file]))[1] ?? "").replace(/\r/g, "");
    const want = "Invoice A|page=1\nInvoice B|page=2\n  Details|page=3\n";
    if (toc !== want) throw new Error(`OFD package ToC: expected ${JSON.stringify(want)}, got ${JSON.stringify(toc)}`);
  });
}

// --- an OFD attachment named .zip opens from memory as OFD, not as a comic book of its images

async function testMemory(): Promise<void> {
  const ofd = makeOfd().toString("latin1");
  const pdf = assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /FileAttachment /P 3 0 R /Rect [200 400 400 600] /FS 5 0 R /Name /PushPin >>",
    "<< /Type /Filespec /F (invoice.zip) /UF (invoice.zip) /EF << /F 6 0 R >> >>",
    `<< /Type /EmbeddedFile /Length ${ofd.length} /Params << /Size ${ofd.length} >> >>\nstream\n${ofd}\nendstream`,
  ]);
  const file = tmpPath("ofd-attachment.pdf");
  writeFileSync(file, pdf, "latin1");
  const { proc, client } = await launchControlled([file]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await pollUntil(
      async () => String((await client.request(ControlCommand.TestMarkupAnnots, ["open-embedded", 0, 0]))[1] ?? ""),
      (r) => /OK/.test(r),
      { timeoutMs: 8000 * SLOW_BUILD_FACTOR, error: (r) => `OFD attachment: open-embedded failed: ${r}` },
    );
    // makeOfd() has 2 pages and 1 image: a comic book of it has 1 page
    const info = await pollUntil(
      async () => String((await client.request(ControlCommand.TestPageInfo, []))[1] ?? ""),
      (r) => /pages=2 /.test(r),
      { timeoutMs: 8000 * SLOW_BUILD_FACTOR, error: (r) => `OFD attachment: expected 2 OFD pages: ${r}` },
    );
    console.log(`OFD attachment: ${info.split(" ").slice(0, 2).join(" ")}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

// --- signatures: SES and GB/T 35275 samples from ofdrw (tests/issue-1909-data), as signed and tampered with

const dataDir = join(ROOT, "tests/issue-1909-data");

// replaces one member of an OFD package
function tamper(src: string, member: string, change: (data: Uint8Array) => Uint8Array): Buffer {
  const entries = readZip(readFileSync(join(dataDir, src)));
  const entry = entries.find((e) => e.name === member);
  if (!entry) throw new Error(`OFD signatures: ${src} has no ${member}`);
  entry.data = change(entry.data);
  return makeZip(entries);
}

const flipLastByte = (d: Uint8Array) => {
  const copy = new Uint8Array(d);
  copy[copy.length - 1] ^= 1;
  return copy;
};
const appendSpace = (d: Uint8Array) => new Uint8Array(Buffer.concat([d, Buffer.from(" ")]));

type SigCase = { name: string; file: string; bar: RegExp; details: RegExp[] };

async function signatureState(file: string, args: string[] = []): Promise<string> {
  const { proc, client } = await launchControlled([...args, file]);
  try {
    await client.waitForRenderIdle();
    const state = await pollUntil(
      async () => String((await client.request(ControlCommand.TestSignatureStatus, [""]))[1] ?? ""),
      (r) => !/rows=0/.test(r),
      { timeoutMs: 8000 * SLOW_BUILD_FACTOR, error: (r) => `OFD signatures: no signature rows: ${r}` },
    );
    const dialog = String((await client.request(ControlCommand.TestSignatureStatus, ["open 0"]))[1] ?? "");
    return `${state.split("\n")[0]}\n${dialog.replace(/\r/g, "")}`;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

async function testSignatures(): Promise<void> {
  // ng has no signature bar or signature dialog yet (orig's SignatureUI.cpp),
  // so there is nothing for TestSignatureStatus to read
  if (USE_NG) return;
  const write = (name: string, data: Buffer) => {
    const path = tmpPath(`ofd-sig-${name}.ofd`);
    writeFileSync(path, data);
    return path;
  };
  const intact = /The document wasn't changed after it was signed \(5 files checked\)/;
  const unknown = /bar=1 At least one signature has problems/;
  const invalid = /bar=1 At least one signature is invalid/;
  const cases: SigCase[] = [
    {
      name: "ses-v4",
      file: join(dataDir, "SESV4SignDoc.ofd"),
      bar: unknown,
      details: [intact, /Seal: OFD R&W 测试用印章/, /certificate wasn't valid at the signature time/],
    },
    { name: "ses-v1", file: join(dataDir, "SESV1SignDoc.ofd"), bar: unknown, details: [intact] },
    {
      name: "gbt35275",
      file: join(dataDir, "GB35275PKCS9DigitalSign.ofd"),
      bar: unknown,
      details: [intact, /Format: digital signature \(GB\/T 35275\)/],
    },
    { name: "no-cert", file: join(dataDir, "DigitalSign.ofd"), bar: unknown, details: [/has no certificate/] },
    {
      name: "changed-page",
      file: write("changed-page", tamper("SESV4SignDoc.ofd", "Doc_0/Pages/Page_0/Content.xml", appendSpace)),
      bar: invalid,
      details: [/Doc_0\/Pages\/Page_0\/Content.xml was changed or removed after signing/],
    },
    {
      name: "changed-signature-xml",
      file: write("changed-sigxml", tamper("SESV4SignDoc.ofd", "Doc_0/Signs/Sign_0/Signature.xml", appendSpace)),
      bar: invalid,
      details: [/Signature.xml was changed after signing/],
    },
    {
      name: "bad-signature-value",
      file: write("bad-value", tamper("SESV4SignDoc.ofd", "Doc_0/Signs/Sign_0/SignedValue.dat", flipLastByte)),
      bar: invalid,
      details: [/the signature doesn't match the signer's certificate/],
    },
  ];
  const failures: string[] = [];
  for (const c of cases) {
    const state = await signatureState(c.file);
    const missing = [c.bar, ...c.details].filter((re) => !re.test(state));
    console.log(`OFD signature ${c.name}: ${missing.length ? "FAILED" : "ok"}`);
    if (missing.length) failures.push(`${c.name}: expected ${missing.join(", ")} in:\n${state}`);
  }
  if (failures.length) throw new Error(failures.join("\n\n"));
}

// --- encrypted packages, integrity protection, trusted roots, links, attachments, conversion

async function testEncrypted(): Promise<void> {
  const file = join(dataDir, "encrypted.ofd");
  // without the password nothing opens
  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestRenderPageColors, [file, 1, 100]);
    if (res[0] === 0) throw new Error(`OFD encrypted: opened without a password: ${res[1]}`);
  });
  const { proc, client } = await launchControlled(["-pwd", "sumatra", file]);
  try {
    const info = await pollUntil(
      async () => String((await client.request(ControlCommand.TestPageInfo, []))[1] ?? ""),
      (r) => /pages=2 /.test(r),
      { timeoutMs: 8000 * SLOW_BUILD_FACTOR, error: (r) => `OFD encrypted: expected 2 pages with the password: ${r}` },
    );
    console.log(`OFD encrypted: ${info.split(" ")[0]}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

async function testIntegrityAndTrust(): Promise<void> {
  // like testSignatures: ng has no signature bar to read
  if (USE_NG) return;
  const withExtra = tmpPath("ofd-integrity-extra.ofd");
  const entries = readZip(readFileSync(join(dataDir, "hello-integrity.ofd")));
  entries.push({ name: "Doc_0/added.xml", data: Buffer.from("<added/>") });
  writeFileSync(withExtra, makeZip(entries));
  const appdata = writeAppdata(
    "ofd-trust",
    `OfdTrustedCertificates = "${join(dataDir, "chain-root.pem").replace(/\\/g, "/")}"`,
  );
  const cases: SigCase[] = [
    {
      name: "integrity",
      file: join(dataDir, "hello-integrity.ofd"),
      bar: /bar=1 At least one signature has problems/,
      details: [/The package has the files it had when it was protected \(5 files\)/],
    },
    {
      name: "integrity-added-file",
      file: withExtra,
      bar: /bar=1 At least one signature is invalid/,
      details: [/1 files aren't covered by the protection, e.g. Doc_0\/added.xml/],
    },
    {
      name: "untrusted-chain",
      file: join(dataDir, "chain-signed.ofd"),
      bar: /bar=1 At least one signature has problems/,
      details: [/doesn't chain to a trusted root/],
    },
  ];
  const failures: string[] = [];
  const check = (c: SigCase, state: string) => {
    const missing = [c.bar, ...c.details].filter((re) => !re.test(state));
    console.log(`OFD signature ${c.name}: ${missing.length ? "FAILED" : "ok"}`);
    if (missing.length) failures.push(`${c.name}: expected ${missing.join(", ")} in:\n${state}`);
  };
  for (const c of cases) check(c, await signatureState(c.file));
  check(
    {
      name: "trusted-chain",
      file: "",
      bar: /bar=1 Signed and all signatures are valid/,
      details: [/chains to the trusted root SumatraPDF Test Root/],
    },
    await signatureState(join(dataDir, "chain-signed.ofd"), ["-appdata", appdata]),
  );
  if (failures.length) throw new Error(failures.join("\n\n"));
}

// page 1 links to page 2 and to a URL; the package has an attachment
function linksFixture(): Buffer {
  const action = (inner: string) => `<Actions><Action Event="CLICK">${inner}</Action></Actions>`;
  const area = (id: number, y: number, inner: string) =>
    `<PathObject ID="${id}" Boundary="5 ${y} 30 8" Visible="false">${action(inner)}` +
    `<AbbreviatedData>M 0 0 L 30 0 L 30 8 L 0 8 C</AbbreviatedData></PathObject>`;
  const files: [string, string | Buffer][] = [
    ["OFD.xml", `<OFD ${ns}><DocBody><DocRoot>Doc/main.xml</DocRoot></DocBody></OFD>`],
    [
      "Doc/main.xml",
      `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 50 50</PhysicalBox></PageArea></CommonData>` +
        `<Pages><Page ID="1" BaseLoc="p1.xml"/><Page ID="2" BaseLoc="p2.xml"/></Pages>` +
        `<Attachments>Attachs/Attachments.xml</Attachments></Document>`,
    ],
    [
      "Doc/p1.xml",
      `<Page ${ns}><Content><Layer>` +
        area(10, 5, `<Goto><Dest Type="XYZ" PageID="2" Left="0" Top="10"/></Goto>`) +
        area(11, 20, `<URI URI="https://www.sumatrapdfreader.org/"/>`) +
        `</Layer></Content></Page>`,
    ],
    ["Doc/p2.xml", `<Page ${ns}><Content/></Page>`],
    [
      "Doc/Attachs/Attachments.xml",
      `<Attachments ${ns}><Attachment ID="20" Name="invoice" Format="xml"><FileLoc>invoice.xml</FileLoc></Attachment></Attachments>`,
    ],
    ["Doc/Attachs/invoice.xml", "<invoice/>"],
  ];
  return makeZip(files.map(([name, data]) => ({ name, data: Buffer.from(data) })));
}

async function testLinksAttachmentsConvert(): Promise<void> {
  const file = tmpPath("ofd-links.ofd");
  writeFileSync(file, linksFixture());
  const pdf = tmpPath("ofd-converted.pdf");
  await withControlledSumatra(EXE, async (client) => {
    const links = String((await client.request(ControlCommand.TestPageLinks, [file, 1]))[1] ?? "");
    if (!/page=2 /.test(links) || !links.includes("https://www.sumatrapdfreader.org/")) {
      throw new Error(`OFD links: expected a link to page 2 and a URL:\n${links}`);
    }
    const toc = String((await client.request(ControlCommand.TestGetToc, [file]))[1] ?? "");
    if (!toc.includes("invoice.xml")) throw new Error(`OFD attachments: expected invoice.xml in:\n${toc}`);

    // Convert to PDF keeps OFD text as text
    const res = await client.request(ControlCommand.TestConvertToPdf, [
      join(dataDir, "../issue-1909-data/chain-signed.ofd"),
      pdf,
    ]);
    if (res[0] !== 0) throw new Error(`OFD to PDF: ${res[1]}`);
    const render = String((await client.request(ControlCommand.TestRenderPageColors, [pdf, 1, 100]))[1] ?? "");
    if (!/blue=[1-9]\d{3,}/.test(render)) throw new Error(`OFD to PDF: page content missing: ${render}`);
  });
  console.log("OFD links, attachments and conversion: ok");
}

export async function testit(): Promise<void> {
  await testBasic();
  await testPathFill();
  await testSeals();
  await testFeatures();
  await testPackage();
  await testMemory();
  await testSignatures();
  await testEncrypted();
  await testIntegrityAndTrust();
  await testLinksAttachmentsConvert();
}

if (import.meta.main) {
  await runStandalone(testit);
}
