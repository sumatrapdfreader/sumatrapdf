// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6132
//
// Document Timestamp signatures (PAdES RFC 3161) without an /M entry in the
// signature dictionary must display the timestamp time and policy ID from
// the TimeStampToken's TSTInfo.
//
// Run: bun tests/issue-6132.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

async function signaturesOf(pdf: string): Promise<string> {
  return withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 20_000;
      let raw = "";
      for (;;) {
        const res = await client.request(ControlCommand.TestDocumentSignatures);
        const code = res[0] as number;
        raw = String(res[1] ?? "");
        if (code === 0) {
          return raw;
        }
        if (code !== 2) {
          throw new Error(`issue-6132 signatures: ${raw.trim()}`);
        }
        if (Date.now() > deadline) {
          throw new Error(`issue-6132: signatures never ready: ${raw.trim()}`);
        }
        await new Promise((r) => setTimeout(r, 150));
      }
    },
    [pdf],
  );
}

// ETSI EN 319 142-1 5.4.3: a reader ignores a document timestamp's M, Reason,
// Location and ContactInfo; the time comes from the token. Copy issue-6132.pdf
// with an incremental update that gives the timestamp (object 177) a bogus /M
// and /Reason.
function withBogusDocTsKeys(src: string): string {
  const pdf = readFileSync(src);
  const s = pdf.toString("latin1");
  const start = s.indexOf("177 0 obj");
  const end = s.indexOf("endobj", start) + "endobj".length;
  const obj = s
    .slice(start, end)
    .replace("/Type/DocTimeStamp", "/Type/DocTimeStamp/M(D:20010101120000Z)/Reason(bogus reason)");
  const prevXref = /startxref\s+(\d+)\s+%%EOF\s*$/.exec(s)![1];
  const trailer = /trailer\s*<<([\s\S]*?)>>\s*startxref/
    .exec(s.slice(s.lastIndexOf("trailer")))![1]
    .replace(/\/Prev \d+/, `/Prev ${prevXref}`);
  const objOff = pdf.length + 1;
  const body = `\n${obj}\n`;
  const xrefOff = pdf.length + body.length;
  const xrefEntry = `${String(objOff).padStart(10, "0")} 00000 n \n`;
  const tail = `xref\n177 1\n${xrefEntry}trailer\n<<${trailer}>>\nstartxref\n${xrefOff}\n%%EOF\n`;
  const out = tmpPath("issue-6132-bogus-m.pdf");
  writeFileSync(out, Buffer.concat([pdf, Buffer.from(body + tail, "latin1")]));
  return out;
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-6132: signature time and policy come from the Windows PKCS#7 verifier");
    return;
  }
  const pdf = join(ROOT, "tests", "issue-6132.pdf");
  const raw = await signaturesOf(pdf);

  const sig2Match = raw.match(/Signature 2 \(document timestamp\):[\s\S]*?(?=\n\n|$)/);
  if (!sig2Match) {
    throw new Error(`issue-6132: missing Signature 2 (document timestamp):\n${raw}`);
  }
  const sig2Text = sig2Match[0];
  if (!/Signature time:\s*2026\/09\/04 06:07:41 UTC/.test(sig2Text)) {
    throw new Error(`issue-6132: Signature 2 missing or wrong signature time:\n${sig2Text}`);
  }
  if (!/The time and date displayed is from the secure time & date server\./.test(sig2Text)) {
    throw new Error(`issue-6132: Signature 2 missing secure time server notice:\n${sig2Text}`);
  }
  if (!/Policy ID:\s*0\.4\.0\.2023\.1\.1/.test(sig2Text)) {
    throw new Error(`issue-6132: Signature 2 missing Policy ID:\n${sig2Text}`);
  }

  const bogus = await signaturesOf(withBogusDocTsKeys(pdf));
  const ts = bogus.match(/Signature 2 \(document timestamp\):[\s\S]*?(?=\n\n|$)/)?.[0] ?? "";
  if (!/Signature time:\s*2026\/09\/04 06:07:41 UTC/.test(ts)) {
    throw new Error(`issue-6132: a document timestamp with /M must show the token's time:\n${ts || bogus}`);
  }
  if (/2001/.test(ts) || /bogus reason/.test(ts)) {
    throw new Error(`issue-6132: a document timestamp shows its ignored /M or /Reason:\n${ts}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
