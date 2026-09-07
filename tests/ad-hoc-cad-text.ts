// Compare the reported CAD labels with enhancement off and on.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const PDF = "C:/Users/kjk/Downloads/Зал центральний ще новіше.1.pdf";
const PAGE_WIDTH = 1684;
const PAGE_HEIGHT = 1191;
const LABELS = [
  { name: "Стадія", rect: [1439, 1028, 1483, 1040] },
  { name: "Сервірувальна", rect: [467, 312, 614, 333] },
];

async function render(mode: string): Promise<Buffer> {
  const dir = tmpPath(`cad-text-${mode}`);
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `EngineeringDrawingEnhance = ${mode}\n`);
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.setNotificationsEnabled(false);
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestConvertToImages, [join(dir, "page-<N>.bmp"), "all"]);
      if (res[0] !== 0 || !String(res[1]).startsWith("OK")) {
        throw new Error(`CAD ${mode}: ${res}`);
      }
    },
    ["-appdata", dir, PDF],
  );
  return readFileSync(join(dir, "page-1.bmp"));
}

function labelPixels(bmp: Buffer, rect: number[]): boolean[] {
  const offset = bmp.readUInt32LE(10);
  const width = bmp.readInt32LE(18);
  const height = bmp.readInt32LE(22);
  if (
    bmp.toString("ascii", 0, 2) !== "BM" ||
    bmp.readUInt16LE(28) !== 32 ||
    bmp.readUInt32LE(30) !== 0 ||
    height <= 0
  ) {
    throw new Error("Expected an uncompressed bottom-up 32-bit BMP");
  }
  const [x0, y0, x1, y1] = rect;
  const pixels: boolean[] = [];
  for (let y = Math.floor((y0! * height) / PAGE_HEIGHT); y < Math.ceil((y1! * height) / PAGE_HEIGHT); y++) {
    for (let x = Math.floor((x0! * width) / PAGE_WIDTH); x < Math.ceil((x1! * width) / PAGE_WIDTH); x++) {
      const pos = offset + ((height - 1 - y) * width + x) * 4;
      pixels.push(bmp[pos]! < 128 && bmp[pos + 1]! < 128 && bmp[pos + 2]! < 128);
    }
  }
  return pixels;
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP: CAD text reproduction requires ${PDF}`);
    return;
  }
  const off = await render("off");
  const on = await render("on");
  const failures: string[] = [];
  for (const label of LABELS) {
    const expected = labelPixels(off, label.rect);
    const actual = labelPixels(on, label.rect);
    const ink = expected.filter(Boolean).length;
    const changed = expected.filter((pixel, i) => pixel !== actual[i]).length;
    console.log(`${label.name}: reference ink=${ink}, changed=${changed}`);
    if (ink < 50 || actual.length !== expected.length || changed > ink * 0.05) {
      failures.push(label.name);
    }
  }
  if (failures.length) {
    throw new Error(`CAD enhancement moved or lost text: ${failures.join(", ")}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
