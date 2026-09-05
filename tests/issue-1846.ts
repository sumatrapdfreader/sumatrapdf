// #1846: incremental update that omits trailer /Info must still show the
// previous Info dict (Acrobat). A rewrite with no Info stays empty.
//
// Fixtures: tests/issue-1846.producer.pdf, tests/issue-1846.stripped.pdf
// Run: bun tests/issue-1846.ts [--no-build]

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
          throw new Error(`issue-1846 properties: ${raw.trim()}`);
        }
        if (Date.now() > deadline) {
          throw new Error(`issue-1846: properties never ready: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 150));
      }
    },
    [pdf],
  );
}

export async function testit(): Promise<void> {
  const producerPdf = join(ROOT, "tests", "issue-1846.producer.pdf");
  const strippedPdf = join(ROOT, "tests", "issue-1846.stripped.pdf");

  const producer = await propertiesOf(producerPdf);
  if (!producer.pdfProducer?.includes("iText")) {
    throw new Error(`issue-1846: expected inherited Producer, got ${JSON.stringify(producer)}`);
  }
  if (!producer.creationDate?.includes("20201226")) {
    throw new Error(`issue-1846: expected inherited CreationDate, got ${JSON.stringify(producer)}`);
  }
  if (!producer.modDate?.includes("20201226")) {
    throw new Error(`issue-1846: expected inherited ModDate, got ${JSON.stringify(producer)}`);
  }

  const stripped = await propertiesOf(strippedPdf);
  if (stripped.pdfProducer || stripped.creationDate || stripped.modDate) {
    throw new Error(`issue-1846: stripped PDF should have no Info, got ${JSON.stringify(stripped)}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
