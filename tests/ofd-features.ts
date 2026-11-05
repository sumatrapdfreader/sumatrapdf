// Focused fixtures for the OFDRW rendering cases, without copying their documents.
// Run: bun tests/ofd-features.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, extractPageText, makePng, makeZip, ROOT, runStandalone, tmpPath } from "./util.ts";

const ns = 'xmlns="http://www.ofdspec.org/2016"';
const rectangle = (w: number, h: number) => `M 0 0 L ${w} 0 L ${w} ${h} L 0 ${h} C`;
const path = (data: string, attrs: string, fill: string, clips = "") =>
  `<PathObject ID="3" Boundary="4 4 20 20" Fill="true" Stroke="false" ${attrs}>` +
  `<FillColor>${fill}</FillColor>${clips}<AbbreviatedData>${data}</AbbreviatedData></PathObject>`;
const red =
  `<PathObject ID="4" Boundary="0 0 20 20" Fill="true" Stroke="false">` +
  `<FillColor Value="255 0 0"/><AbbreviatedData>${rectangle(20, 20)}</AbbreviatedData></PathObject>`;

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
    resources: `<CompositeGraphicUnits><CompositeGraphicUnit ID="10" Width="20" Height="20"><Content>${red}</Content></CompositeGraphicUnit></CompositeGraphicUnits>`,
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
];

function fixture(c: Case): Buffer {
  const files: [string, Buffer][] = [
    ["OFD.xml", Buffer.from(`<OFD ${ns}><DocBody><DocRoot>Doc/main.xml</DocRoot></DocBody></OFD>`)],
    [
      "Doc/main.xml",
      Buffer.from(
        `<Document ${ns}><CommonData><PageArea><PhysicalBox>0 0 30 30</PhysicalBox></PageArea>` +
          `${c.common ?? ""}<DocumentRes>Res.xml</DocumentRes></CommonData><Pages><Page ID="1" BaseLoc="page.xml"/></Pages></Document>`,
      ),
    ],
    ["Doc/Res.xml", Buffer.from(`<Res ${ns} BaseLoc="${c.base ?? "."}">${c.resources ?? ""}</Res>`)],
    ["Doc/page.xml", Buffer.from(`<Page ${ns}><Content><Layer>${c.body}</Layer></Content></Page>`)],
  ];
  if (c.font) files.push(["Doc/font.cff", c.font]);
  if (c.image) files.push(["Doc/Assets/blue.png", c.image]);
  return makeZip(files.map(([name, data]) => ({ name, data })));
}

export async function testit(): Promise<void> {
  const failures: string[] = [];
  const files = cases.map((c) => {
    const file = tmpPath(`ofd-feature-${c.name}.ofd`);
    writeFileSync(file, fixture(c));
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

if (import.meta.main) {
  await runStandalone(testit);
}
