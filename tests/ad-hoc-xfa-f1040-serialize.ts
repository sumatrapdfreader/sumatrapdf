// Ad-hoc: verify hybrid IRS f1040 datasets serialize with bound field values.
//
// Run:  bun tests/ad-hoc-xfa-f1040-serialize.ts [--no-build]

import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import {
  queryXfa,
  queryXfaSerializeData,
  queryXfaSetFieldSerializeData,
} from "./xfa-test-util.ts";
import { runStandalone } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const PDF = join(here, "ad-hoc-xfa-data", "ad-hoc-f1040.pdf");

const MARKERS = [
  "<datasets",
  "<topmostSubform",
  "<c1_01>0</c1_01>",
  "<c1_11>0</c1_11>",
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
  if (xfa.unbound_field_names !== "-") {
    throw new Error(`expected unbound_field_names=-, got ${xfa.unbound_field_names}`);
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

  const changed = await queryXfaSetFieldSerializeData(PDF, "c1_11", "1");
  if (!changed.includes("<c1_11>1</c1_11>")) {
    throw new Error("serialized datasets missing updated marker: <c1_11>1</c1_11>");
  }
  if (changed.includes("<c1_11>0</c1_11>")) {
    throw new Error("serialized datasets still contains stale marker: <c1_11>0</c1_11>");
  }

  console.log(
    `ad-hoc-f1040-serialize: OK bytes=${xml.length} fields_bound=${xfa.fields_bound} markers=${MARKERS.length} round_trip=c1_11`,
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}