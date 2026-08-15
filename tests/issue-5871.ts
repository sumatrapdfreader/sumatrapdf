// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5871
//
// Condensed title/header fonts (MyriadPro-BoldCond on this CompTIA page) use
// real word spaces at ~0.16em. The #5627 tracking-space filter originally
// dropped any space under 0.2em (and synthetic spaces under 0.35em), so headers
// copied as "CompTIAA+CertificationAll-in-OneExamGuide" and double-click word
// select treated the whole run as one word.
//
// #5868 lowered the single threshold to 0.1em (tracking is ~0.00-0.02em; body /
// TJ word spaces are ~0.20-0.30em). This fixture locks the condensed-header case
// so a future threshold tweak cannot re-break #5871 while still killing #5627.
//
// Fixture: tests/issue-5871.pdf (one page from the issue attachment).
//
// Run:  bun tests/issue-5871.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync } from "node:fs";
import { join } from "node:path";
import { extractPageText, runStandalone } from "./util.ts";

const PDF = join(import.meta.dir, "issue-5871.pdf");

// Phrases that lose their spaces if the tracking filter is too aggressive
const WANTED = [
  "CompTIA A+ Certification All-in-One Exam Guide",
  "IEEE 802.11-Based Wireless Networking",
  "Wireless Networking Standards and Regulations",
  "EXAM TIP WPA2",
  "WPA2 Wi-Fi Protected Access 2",
];

// What the same headers look like when every word space is deleted
const BAD = [
  "CompTIAA+Certification",
  "IEEE802.11-BasedWirelessNetworking",
  "WirelessNetworkingStandardsandRegulations",
];

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP issue-5871: fixture not found: ${PDF}`);
    return;
  }

  const text = extractPageText(PDF);
  console.log(`extracted ${text.length} chars`);

  for (const bad of BAD) {
    if (text.includes(bad)) {
      throw new Error(
        `header word spaces were dropped (issue #5871 regressed): found '${bad}'`,
      );
    }
  }
  for (const w of WANTED) {
    if (!text.includes(w)) {
      throw new Error(
        `extracted text is missing word spacing: expected '${w}' (issue #5871)`,
      );
    }
  }
  console.log(`PASS: condensed headers keep word spaces (issue #5871)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
