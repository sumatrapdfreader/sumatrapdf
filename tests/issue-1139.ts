// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1139
//
// TocWrapText: a long bookmark wraps to the sidebar's width, its row as many
// rows tall as its lines; short ones keep one row. Without the setting every
// row is one row tall.
//
// Run:  bun tests/issue-1139.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { assemblePdf, EXE, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

const kLong =
  "A very long bookmark title that cannot possibly fit on a single line of the sidebar and has to wrap over several lines";
const kTitles = ["Short", kLong, "End"];

// one bookmark per page
function makePdf(): string {
  const n = kTitles.length;
  const firstOutline = 4;
  const firstPage = firstOutline + n;
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R /Outlines 3 0 R >>";
  const kids = kTitles.map((_, i) => `${firstPage + i} 0 R`).join(" ");
  objs[2] = `<< /Type /Pages /Count ${n} /Kids [${kids}] >>`;
  objs[3] = `<< /Type /Outlines /First ${firstOutline} 0 R /Last ${firstOutline + n - 1} 0 R /Count ${n} >>`;
  kTitles.forEach((t, i) => {
    const prev = i > 0 ? `/Prev ${firstOutline + i - 1} 0 R ` : "";
    const next = i < n - 1 ? `/Next ${firstOutline + i + 1} 0 R ` : "";
    objs[firstOutline + i] = `<< /Title (${t}) /Parent 3 0 R ${prev}${next}/Dest [${firstPage + i} 0 R /Fit] >>`;
    objs[firstPage + i] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>";
  });
  return assemblePdf(objs.slice(1));
}

type Rows = { rowDy: number; rows: Map<string, number> };

// "OK rowDy=18 width=240 wrap=1\nrow y=0 dy=36 title=..."
async function tocRows(client: ControlClient): Promise<Rows | null> {
  const [code, raw] = await client.request(ControlCommand.TestTocRows, []);
  const s = String(raw ?? "");
  const m = /rowDy=(\d+)/.exec(s);
  if (code !== 0 || !m) {
    return null;
  }
  const rows = new Map<string, number>();
  for (const line of s.split("\n")) {
    const r = /^row y=-?\d+ dy=(\d+) title=(.*)$/.exec(line.trim());
    if (r) {
      rows.set(r[2], Number(r[1]));
    }
  }
  return { rowDy: Number(m[1]), rows };
}

async function withToc(name: string, wrap: boolean, fn: (r: Rows) => void, sidebarDx = 0): Promise<void> {
  const dir = tmpPath(`issue-1139-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-1139.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `RestoreSession = false\nShowToc = true\nTocWrapText = ${wrap}\nSidebarDx = ${sidebarDx}\n`,
  );
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      let r: Rows | null = null;
      for (let i = 0; i < 50; i++) {
        r = await tocRows(client);
        if (r && r.rows.size === kTitles.length) {
          break;
        }
        await sleep(100);
      }
      if (!r || r.rows.size !== kTitles.length) {
        throw new Error(`issue-1139: ${name}: bookmarks not shown: ${JSON.stringify(r && [...r.rows])}`);
      }
      fn(r);
    },
    ["-appdata", dir, pdf],
  );
}

export async function testit(): Promise<void> {
  await withToc("wrap", true, (r) => {
    const long = r.rows.get(kLong)!;
    if (long < 2 * r.rowDy || long % r.rowDy !== 0) {
      throw new Error(`issue-1139: the long bookmark is ${long}px, want 2 or more rows of ${r.rowDy}`);
    }
    for (const t of ["Short", "End"]) {
      if (r.rows.get(t) !== r.rowDy) {
        throw new Error(`issue-1139: "${t}" is ${r.rows.get(t)}px, want one row of ${r.rowDy}`);
      }
    }
  });

  // a wider sidebar: fewer rows
  let narrowRows = 0;
  await withToc("narrow", true, (r) => {
    narrowRows = r.rows.get(kLong)! / r.rowDy;
  });
  await withToc(
    "wide",
    true,
    (r) => {
      const wideRows = r.rows.get(kLong)! / r.rowDy;
      if (wideRows >= narrowRows) {
        throw new Error(`issue-1139: the long bookmark is ${wideRows} rows wide, ${narrowRows} narrow`);
      }
    },
    600,
  );

  await withToc("nowrap", false, (r) => {
    for (const [t, dy] of r.rows) {
      if (dy !== r.rowDy) {
        throw new Error(`issue-1139: without TocWrapText "${t}" is ${dy}px, want ${r.rowDy}`);
      }
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
