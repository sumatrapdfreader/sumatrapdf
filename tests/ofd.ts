// Native OFD: templates, inherited paint, repeated character spacing and annotation images.
// Run: bun tests/ofd.ts [--no-build]

import { copyFileSync, writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, extractPageText, makePng, makeZip, runStandalone, tmpPath } from "./util.ts";

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

export async function testit(): Promise<void> {
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

if (import.meta.main) {
  await runStandalone(testit);
}
