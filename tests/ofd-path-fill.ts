// Path fill defaults to transparent; text fill defaults to black.
// Run: bun tests/ofd-path-fill.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, loadPng, makeZip, pngPixel, runStandalone, tmpPath } from "./util.ts";

const dpi = 720;
const mmPerInch = 25.4;

function makeSymbol(): Buffer {
  const ns = 'xmlns="http://www.ofdspec.org/2016"';
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

export async function testit(): Promise<void> {
  const file = tmpPath("ofd-path-fill.ofd");
  const output = tmpPath("ofd-path-fill-1.png");
  writeFileSync(file, makeSymbol());
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const result = await client.request(ControlCommand.TestConvertToImages, [output, "1", dpi]);
      if (result[0] !== 0 || !String(result[1]).startsWith("OK")) {
        throw new Error(`OFD symbol export failed: ${result}`);
      }
    },
    [file],
  );

  const image = loadPng(output);
  const pixel = (x: number, y: number) =>
    pngPixel(image, Math.round((x * dpi) / mmPerInch), Math.round((y * dpi) / mmPerInch));
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

if (import.meta.main) {
  await runStandalone(testit);
}
