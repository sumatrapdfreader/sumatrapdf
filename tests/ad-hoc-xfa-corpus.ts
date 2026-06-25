// Ad-hoc corpus: real-world XFA PDFs probed via TestXfa.
//
// Fixtures in tests/ad-hoc-xfa-data/ (issue-1294-xfa.pdf from SumatraPDF regression
// suite; ad-hoc-f1040.pdf hybrid IRS form). Optional entries use SUMATRA_SUMTEST or
// OneDrive sumtest paths when present.
//
// Baselines record current engine behavior; tighten expectations as XFA support grows.
//
// Run:  bun tests/ad-hoc-xfa-corpus.ts [--no-build]

import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { formatXfaInfo, queryXfa, type XfaInfo } from "./xfa-test-util.ts";
import { runStandalone } from "./util.ts";

const here = dirname(fileURLToPath(import.meta.url));
const dataDir = join(here, "ad-hoc-xfa-data");

type CorpusExpect = Pick<XfaInfo, "has_xfa" | "pure_xfa" | "valid"> & {
  min_page_count?: number;
  min_render_fields?: number;
  min_p1_fields?: number;
  min_font_families?: number;
  max_font_missing?: number;
};

type CorpusEntry = {
  name: string;
  path: string;
  expect: CorpusExpect;
  optional?: boolean;
};

function sumtestPath(...parts: string[]): string {
  const root = process.env.SUMATRA_SUMTEST ?? "C:/Users/kjk/OneDrive/!sumatra/sumtest";
  return join(root, ...parts);
}

const corpus: CorpusEntry[] = [
  {
    name: "issue-1294-xfa",
    path: join(dataDir, "issue-1294-xfa.pdf"),
    // Pure-XFA regression sample (GitHub #1294).
    expect: { has_xfa: 1, pure_xfa: 1, valid: 1, min_page_count: 1 },
  },
  {
    name: "ad-hoc-f1040",
    path: join(dataDir, "ad-hoc-f1040.pdf"),
    // Hybrid IRS 1040: AcroForm + XFA packet (116 fields: 69 on page 0, 47 on page 1).
    expect: {
      has_xfa: 1,
      pure_xfa: 0,
      valid: 1,
      min_page_count: 2,
      min_render_fields: 69,
      min_p1_fields: 47,
      min_font_families: 12,
      max_font_missing: 1,
    },
  },
  {
    name: "transfer-auth-form",
    path: sumtestPath("formats", "pdf", "Transfer_Authorization_Form.pdf"),
    optional: true,
    expect: { has_xfa: 1, pure_xfa: 0, valid: 1, min_page_count: 1 },
  },
];

function assertCorpus(name: string, xfa: XfaInfo, expect: CorpusExpect): void {
  if (xfa.has_xfa !== expect.has_xfa) {
    throw new Error(`${name}: expected has_xfa=${expect.has_xfa}, got has_xfa=${xfa.has_xfa}`);
  }
  if (xfa.pure_xfa !== expect.pure_xfa) {
    throw new Error(`${name}: expected pure_xfa=${expect.pure_xfa}, got pure_xfa=${xfa.pure_xfa}`);
  }
  if (xfa.valid !== expect.valid) {
    throw new Error(`${name}: expected valid=${expect.valid}, got valid=${xfa.valid}`);
  }
  if (expect.min_page_count !== undefined && xfa.page_count < expect.min_page_count) {
    throw new Error(
      `${name}: expected page_count>=${expect.min_page_count}, got page_count=${xfa.page_count}`,
    );
  }
  if (expect.min_render_fields !== undefined && xfa.render_fields < expect.min_render_fields) {
    throw new Error(
      `${name}: expected render_fields>=${expect.min_render_fields}, got render_fields=${xfa.render_fields}`,
    );
  }
  if (expect.min_p1_fields !== undefined && xfa.p1_fields < expect.min_p1_fields) {
    throw new Error(
      `${name}: expected p1_fields>=${expect.min_p1_fields}, got p1_fields=${xfa.p1_fields}`,
    );
  }
  if (expect.min_font_families !== undefined && xfa.font_families < expect.min_font_families) {
    throw new Error(
      `${name}: expected font_families>=${expect.min_font_families}, got font_families=${xfa.font_families}`,
    );
  }
  if (expect.max_font_missing !== undefined && xfa.font_missing > expect.max_font_missing) {
    throw new Error(
      `${name}: expected font_missing<=${expect.max_font_missing}, got font_missing=${xfa.font_missing}`,
    );
  }
}

export async function testit(): Promise<void> {
  let ran = 0;
  let skipped = 0;

  for (const entry of corpus) {
    if (!existsSync(entry.path)) {
      if (entry.optional) {
        console.log(`${entry.name}: skip (optional, not found: ${entry.path})`);
        skipped++;
        continue;
      }
      throw new Error(`${entry.name}: required fixture missing: ${entry.path}`);
    }

    const xfa = await queryXfa(entry.path);
    console.log(formatXfaInfo(entry.name, xfa));
    assertCorpus(entry.name, xfa, entry.expect);
    ran++;
  }

  if (ran === 0) {
    throw new Error("ad-hoc-xfa-corpus: no fixtures ran");
  }
  console.log(`ad-hoc-xfa-corpus: ${ran} passed, ${skipped} skipped`);
}

if (import.meta.main) {
  await runStandalone(testit);
}