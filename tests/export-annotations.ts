// Export Annotations (CmdExportAnnotations): a PDF's annotations as Markdown
// (a .md file) or CSV (a .csv file), with the text a highlight marks and a
// note's contents. The Save As dialog is answered through -dbg-control.
//
// Run: bun tests/export-annotations.ts [--no-build] [-ng]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, EXE, makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const name = "export-annotations";

function cli(args: string[]): void {
  const p = Bun.spawnSync([EXE, "cli", ...args], { stdout: "pipe", stderr: "pipe" });
  if (p.exitCode !== 0) {
    throw new Error(`${name}: cli ${args.join(" ")}: ${p.stderr.toString()}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const plain = join(dir, "plain.pdf");
  writeFileSync(plain, makeTextPdf("hello cli world, a quick note"));
  const highlighted = join(dir, "highlighted.pdf");
  const pdf = join(dir, "marked.pdf");
  cli(["annotate", plain, "highlight", "--text", "cli world", "-o", highlighted]);
  cli(["annotate", highlighted, "note", "--at", "72,72", "--contents", 'check this, "quoted"', "-o", pdf]);

  const md = join(dir, "marked-annotations.md");
  const csv = join(dir, "marked-annotations.csv");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();

    await client.request(ControlCommand.TestSaveFileName, [md]);
    sendCommand(frame, cmdId("CmdExportAnnotations"));
    await pollUntil(
      () => existsSync(md),
      (ok) => ok,
      { error: () => `${name}: no ${md}` },
    );
    const mdText = readFileSync(md, "utf8");
    for (const want of ["# Annotations in marked.pdf", "## Page 1", '**Highlight**: "cli world"', "check this"]) {
      if (!mdText.includes(want)) {
        throw new Error(`${name}: Markdown lacks '${want}':\n${mdText}`);
      }
    }

    await client.request(ControlCommand.TestSaveFileName, [csv]);
    sendCommand(frame, cmdId("CmdExportAnnotations"));
    await pollUntil(
      () => existsSync(csv),
      (ok) => ok,
      { error: () => `${name}: no ${csv}` },
    );
    const lines = readFileSync(csv, "utf8").replace(/^﻿/, "").split("\r\n");
    if (lines[0] !== "page,type,text,note,author,color,modified") {
      throw new Error(`${name}: CSV header '${lines[0]}'`);
    }
    if (!lines.some((l) => l.startsWith("1,Highlight,cli world,"))) {
      throw new Error(`${name}: no highlight row:\n${lines.join("\n")}`);
    }
    // a comma and quotes in a field: quoted, quotes doubled
    if (!lines.some((l) => l.includes('"check this, ""quoted"""'))) {
      throw new Error(`${name}: note not quoted:\n${lines.join("\n")}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
