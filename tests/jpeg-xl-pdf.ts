import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { deflateSync } from "node:zlib";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SIZE = 64;

function makePdf(data: Buffer, filter: string, extra = ""): Buffer {
  const content = Buffer.from(`q ${SIZE} 0 0 ${SIZE} 0 0 cm /Im Do Q`);
  const stream = (dict: string, bytes: Buffer) =>
    Buffer.concat([Buffer.from(`<< ${dict} /Length ${bytes.length} >>\nstream\n`), bytes, Buffer.from("\nendstream")]);
  const objs = [
    Buffer.from("<< /Type /Catalog /Pages 2 0 R >>"),
    Buffer.from("<< /Type /Pages /Kids [3 0 R] /Count 1 >>"),
    Buffer.from(
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${SIZE} ${SIZE}] /Resources << /XObject << /Im 5 0 R >> >> /Contents 4 0 R >>`,
    ),
    stream("", content),
    stream(`/Type /XObject /Subtype /Image /Width ${SIZE} /Height ${SIZE} /Filter ${filter} ${extra}`, data),
  ];
  if (extra.includes("/SMask")) {
    objs.push(
      stream(
        `/Type /XObject /Subtype /Image /Width ${SIZE} /Height ${SIZE} /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /FlateDecode`,
        deflateSync(Buffer.alloc(SIZE * SIZE, 128)),
      ),
    );
  }
  const parts = [Buffer.from("%PDF-2.0\n")];
  const offsets = [0];
  let offset = parts[0]!.length;
  for (const [i, obj] of objs.entries()) {
    offsets.push(offset);
    const part = Buffer.concat([Buffer.from(`${i + 1} 0 obj\n`), obj, Buffer.from("\nendobj\n")]);
    parts.push(part);
    offset += part.length;
  }
  const entries = offsets
    .slice(1)
    .map((n) => `${String(n).padStart(10, "0")} 00000 n \n`)
    .join("");
  parts.push(
    Buffer.from(
      `xref\n0 ${offsets.length}\n0000000000 65535 f \n${entries}trailer\n<< /Size ${offsets.length} /Root 1 0 R >>\nstartxref\n${offset}\n%%EOF\n`,
    ),
  );
  return Buffer.concat(parts);
}

function tool(args: string[], expectedExit = 0): string {
  const p = Bun.spawnSync([EXE, ...args], { stdout: "pipe", stderr: "pipe", timeout: 30_000 });
  if (p.exitCode !== expectedExit) {
    throw new Error(`${args[0]} failed: ${p.stderr.toString()}`);
  }
  return p.stderr.toString();
}

function render(path: string, name: string): Buffer {
  const output = tmpPath(`jpeg-xl-${name}.ppm`);
  tool(["draw", "-o", output, "-r", "72", path]);
  const ppm = readFileSync(output);
  const header = /^P6\s+64\s+64\s+255\s/.exec(ppm.toString("latin1"));
  if (!header) {
    throw new Error(`Unexpected PPM header: ${ppm.subarray(0, 30)}`);
  }
  return ppm.subarray(header[0].length);
}

export async function testit(): Promise<void> {
  const raw = readFileSync(join(import.meta.dir, "issue-6245-data", "keong_macan.jxl"));
  const box = Buffer.alloc(8);
  box.writeUInt32BE(raw.length + box.length);
  box.write("jxlc", 4);
  const container = Buffer.concat([
    Buffer.from("0000000c4a584c200d0a870a00000014667479706a786c20000000006a786c20", "hex"),
    box,
    raw,
  ]);
  const cases = [
    { name: "raw", data: raw, filter: "/JXLDecode", extra: "" },
    { name: "container", data: container, filter: "/JXLDecode", extra: "" },
    {
      name: "flate",
      data: deflateSync(raw),
      filter: "[/FlateDecode /JXLDecode]",
      extra: "/ColorSpace /DeviceRGB /BitsPerComponent 8",
    },
    { name: "invert", data: raw, filter: "/JXLDecode", extra: "/Decode [1 0 1 0 1 0]" },
    { name: "mask", data: raw, filter: "/JXLDecode", extra: "/SMask 6 0 R" },
    { name: "color-key", data: raw, filter: "/JXLDecode", extra: "/Mask [0 255 0 255 0 255]" },
  ];
  let reference: Buffer | undefined;
  for (const c of cases) {
    const pdf = tmpPath(`jpeg-xl-${c.name}.pdf`);
    writeFileSync(pdf, makePdf(c.data, c.filter, c.extra));
    const pixels = render(pdf, c.name);
    const middleRow = (SIZE / 2) * SIZE * 3;
    const left = [...pixels.subarray(middleRow, middleRow + 3)];
    const want =
      c.name === "invert"
        ? [164, 127, 85]
        : c.name === "mask"
          ? [173, 191, 212]
          : c.name === "color-key"
            ? [255, 255, 255]
            : [91, 128, 170];
    if (pixels.length !== SIZE * SIZE * 3 || left.some((n, i) => Math.abs(n - want[i]!) > 6)) {
      throw new Error(`${c.name}: left pixel ${left}, expected ${want}`);
    }
    if (["raw", "container", "flate"].includes(c.name)) {
      reference ??= pixels;
      if (!pixels.equals(reference)) {
        throw new Error(`${c.name}: pixels differ from raw codestream`);
      }
    }
    const saved = tmpPath(`jpeg-xl-${c.name}-saved.pdf`);
    tool(["clean", "-d", pdf, saved]);
    if (!render(saved, `${c.name}-saved`).equals(pixels)) {
      throw new Error(`${c.name}: pixels changed after saving`);
    }
    console.log(`JPEG XL PDF ${c.name}: OK`);
  }
  const image = tmpPath("jpeg-xl-image.jxl");
  writeFileSync(image, raw);
  if (!render(image, "image").equals(reference!)) {
    throw new Error("MuPDF JPEG XL image pixels differ from PDF pixels");
  }
  const broken = tmpPath("jpeg-xl-broken.pdf");
  writeFileSync(broken, makePdf(raw.subarray(0, 12), "/JXLDecode"));
  const error = tool(["draw", "-o", tmpPath("jpeg-xl-broken.ppm"), broken], 1);
  if (!error.includes("JPEG XL")) {
    throw new Error(`Truncated JPEG XL did not report a decode error: ${error}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
