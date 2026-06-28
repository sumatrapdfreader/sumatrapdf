// Tests for str_port_inventory.ts block-comment FSM and param detection.
// Usage: bun cmd/scripts/str_port_inventory_test.ts

import { countScannedLines, scanFileContent } from "./str_port_inventory.ts";

const fixture = `/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

    bool IsClsid(const char* s) {
        return false;
    }
`;

function assert(cond: boolean, msg: string) {
  if (!cond) {
    throw new Error(msg);
  }
}

const scanned = countScannedLines(fixture);
assert(scanned > 0, `expected scanned line count > 0, got ${scanned}`);

const entries = scanFileContent("fixture.cpp", fixture);
const isClsid = entries.find((e) => e.symbol === "s" && e.kind === "param");
assert(isClsid !== undefined, "expected IsClsid const char* param in inventory");
assert(!isClsid.exclusion, `expected untagged param, got exclusion=${isClsid.exclusion}`);

console.log(`str_port_inventory_test: OK (scanned=${scanned}, entries=${entries.length})`);