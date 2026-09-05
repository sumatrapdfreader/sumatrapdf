// Document Properties fill from Catalog /Metadata (XMP) when Info is missing
// those fields. Info wins when both are present.
//
// Fixtures: tests/xmp-properties.pdf, tests/xmp-properties-info.pdf,
// tests/xmp-visagesoft.pdf (real file, no Info).
// Regenerate the first two: bun tests/xmp-properties-make.ts
// Run: bun tests/xmp-properties.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone } from "./util.ts";

function parseProps(raw: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of raw.split(/\r?\n/)) {
    const eq = line.indexOf("=");
    if (eq <= 0) {
      continue;
    }
    out[line.slice(0, eq)] = line.slice(eq + 1);
  }
  return out;
}

async function propertiesOf(pdf: string): Promise<Record<string, string>> {
  return withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 20_000;
      let raw = "";
      for (;;) {
        const res = await client.request(ControlCommand.TestDocumentProperties);
        const code = res[0] as number;
        raw = String(res[1] ?? "");
        if (code === 0) {
          return parseProps(raw);
        }
        if (code !== 2) {
          throw new Error(`xmp-properties: ${raw.trim()}`);
        }
        if (Date.now() > deadline) {
          throw new Error(`xmp-properties: properties never ready: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 150));
      }
    },
    [pdf],
  );
}

function mustContain(props: Record<string, string>, key: string, needle: string): void {
  const val = props[key] ?? "";
  if (!val.includes(needle)) {
    throw new Error(
      `xmp-properties: expected ${key} to contain ${JSON.stringify(needle)}, got ${JSON.stringify(props)}`,
    );
  }
}

export async function testit(): Promise<void> {
  const onlyPdf = join(ROOT, "tests", "xmp-properties.pdf");
  const infoPdf = join(ROOT, "tests", "xmp-properties-info.pdf");
  const realPdf = join(ROOT, "tests", "xmp-visagesoft.pdf");

  const only = await propertiesOf(onlyPdf);
  if (only.title !== "XmpOnlyTitle") {
    throw new Error(`xmp-properties: title, got ${JSON.stringify(only)}`);
  }
  if (only.author !== "XmpAuthorOne, XmpAuthorTwo") {
    throw new Error(`xmp-properties: author, got ${JSON.stringify(only)}`);
  }
  if (only.subject !== "XmpOnlySubject") {
    throw new Error(`xmp-properties: subject, got ${JSON.stringify(only)}`);
  }
  if (only.keywords !== "alpha, beta") {
    throw new Error(`xmp-properties: keywords, got ${JSON.stringify(only)}`);
  }
  if (only.copyright !== "XmpOnlyRights") {
    throw new Error(`xmp-properties: copyright, got ${JSON.stringify(only)}`);
  }
  if (only.creatorApp !== "XmpOnlyTool") {
    throw new Error(`xmp-properties: creatorApp, got ${JSON.stringify(only)}`);
  }
  if (only.pdfProducer !== "XmpOnlyProducer") {
    throw new Error(`xmp-properties: pdfProducer, got ${JSON.stringify(only)}`);
  }
  mustContain(only, "creationDate", "20240315");
  mustContain(only, "modDate", "20240316");

  const info = await propertiesOf(infoPdf);
  if (info.title !== "InfoTitle") {
    throw new Error(`xmp-properties: Info title must win, got ${JSON.stringify(info)}`);
  }
  if (info.pdfProducer !== "InfoProducer") {
    throw new Error(`xmp-properties: Info producer must win, got ${JSON.stringify(info)}`);
  }
  if (info.author !== "XmpAuthor") {
    throw new Error(`xmp-properties: XMP author fill, got ${JSON.stringify(info)}`);
  }
  if (info.creatorApp !== "XmpTool") {
    throw new Error(`xmp-properties: XMP creator fill, got ${JSON.stringify(info)}`);
  }

  const real = await propertiesOf(realPdf);
  mustContain(real, "pdfProducer", "Visagesoft");
  mustContain(real, "modDate", "20200730");
}

if (import.meta.main) {
  await runStandalone(testit);
}
