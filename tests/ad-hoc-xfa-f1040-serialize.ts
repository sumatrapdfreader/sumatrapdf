// Ad-hoc: verify hybrid IRS f1040 datasets serialize with bound field values.
//
// Run:  bun tests/ad-hoc-xfa-f1040-serialize.ts [--no-build]

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { queryXfa, queryXfaSerializeData } from "./xfa-test-util.ts";
import { runStandalone } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-f1040.pdf");

const MARKERS = [
  "<datasets",
  "<topmostSubform",
  "<c1_01>0</c1_01>",
  "<c2_07>0</c2_07>",
  "<FilingStatus>",
  "<f1_01",
  "<Page2>",
];

export async function testit(): Promise<void> {
  const xfa = await queryXfa(PDF);
  if (xfa.serialize_ok !== 1) {
    throw new Error(`expected serialize_ok=1, got ${xfa.serialize_ok}`);
  }
  if (xfa.fields_bound < 116) {
    throw new Error(`expected fields_bound>=116, got ${xfa.fields_bound}`);
  }

  const xml = await queryXfaSerializeData(PDF);
  if (xml.length < 2000) {
    throw new Error(`serialized datasets too short: ${xml.length} bytes`);
  }

  for (const marker of MARKERS) {
    if (!xml.includes(marker)) {
      throw new Error(`serialized datasets missing marker: ${marker}`);
    }
  }

  console.log(
    `ad-hoc-f1040-serialize: OK bytes=${xml.length} fields_bound=${xfa.fields_bound} markers=${MARKERS.length}`,
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}