// #5991: convert PDF pages to images. Destination is a path template where
// <N> is replaced by the 1-based page number; pages can be current, all, or
// a custom range.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
`;

function makeTwoPagePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

function pngSize(buf: Buffer): { w: number; h: number } {
  if (buf.length < 24 || buf[0] !== 0x89 || buf[1] !== 0x50 || buf[2] !== 0x4e || buf[3] !== 0x47) {
    throw new Error(`issue-5991: not a PNG (${buf.length} bytes)`);
  }
  return { w: buf.readUInt32BE(16), h: buf.readUInt32BE(20) };
}

async function convert(client: ControlClient, template: string, pages: string): Promise<string> {
  const deadline = Date.now() + 30_000;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestConvertToImages, [template, pages]);
    const raw = String(res[1] ?? "").trim();
    last = raw;
    if (res[0] === 0 && raw.startsWith("OK")) {
      return raw;
    }
    if (res[0] !== 0 && !raw.includes("NOTREADY")) {
      throw new Error(`issue-5991: convert failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5991: convert timed out: ${last}`);
    }
    await new Promise((r) => setTimeout(r, 50));
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5991");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makeTwoPagePdf());

  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();

      const allDir = join(dir, "all");
      mkdirSync(allDir, { recursive: true });
      const allT = join(allDir, "page-<N>.png");
      const all = await convert(client, allT, "all");
      const p1 = join(allDir, "page-1.png");
      const p2 = join(allDir, "page-2.png");
      if (!existsSync(p1) || !existsSync(p2)) {
        throw new Error(`issue-5991: all pages missing files (${all}): ${p1} ${p2}`);
      }
      if (existsSync(join(allDir, "page-3.png"))) {
        throw new Error("issue-5991: all pages wrote a third file");
      }
      const s1 = pngSize(readFileSync(p1));
      // 612x792 pt at 150 DPI → 1275 x 1650
      if (Math.abs(s1.w - 1275) > 2 || Math.abs(s1.h - 1650) > 2) {
        throw new Error(`issue-5991: unexpected size ${s1.w}x${s1.h} (want ~1275x1650)`);
      }

      const oneDir = join(dir, "one");
      mkdirSync(oneDir, { recursive: true });
      const oneT = join(oneDir, "only-<N>.png");
      await convert(client, oneT, "2");
      if (!existsSync(join(oneDir, "only-2.png"))) {
        throw new Error("issue-5991: custom page 2 did not write only-2.png");
      }
      if (existsSync(join(oneDir, "only-1.png"))) {
        throw new Error("issue-5991: custom page 2 also wrote only-1.png");
      }

      const jpgDir = join(dir, "jpg");
      mkdirSync(jpgDir, { recursive: true });
      await convert(client, join(jpgDir, "p-<N>.jpg"), "1");
      const jpg = join(jpgDir, "p-1.jpg");
      if (!existsSync(jpg)) {
        throw new Error("issue-5991: jpeg convert did not write p-1.jpg");
      }
      const jpgBuf = readFileSync(jpg);
      if (jpgBuf.length < 2 || jpgBuf[0] !== 0xff || jpgBuf[1] !== 0xd8) {
        throw new Error(`issue-5991: p-1.jpg is not a JPEG (${jpgBuf.length} bytes)`);
      }
    },
    ["-appdata", dir, pdf],
  );

  console.log("issue-5991: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
