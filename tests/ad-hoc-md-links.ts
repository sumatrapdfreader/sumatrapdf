// Markdown [text](file.md) links from MuPDF should be launchFile destinations.
// Fixture lives under tests/tmp/md-link-test/ (gitignored).

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const FIXTURE = tmpPath("md-link-test/alpha.md");

function parseLinks(raw: string): { kind: string; value: string }[] {
  return (raw ?? "")
    .trim()
    .split("\n")
    .filter((l) => l.startsWith("kind="))
    .map((l) => {
      const kind = l.slice("kind=".length, l.indexOf(" value="));
      const value = l.slice(l.indexOf(" value=") + " value=".length);
      return { kind, value };
    });
}

export async function testit(): Promise<void> {
  if (!existsSync(FIXTURE)) {
    throw new Error(`missing fixture ${FIXTURE} (create alpha.md with [Beta](beta.md) links)`);
  }

  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestPageLinks, [FIXTURE, 1]);
  if (exitCode !== 0) {
    throw new Error(`TestPageLinks failed: ${(raw ?? "").trim()}`);
  }

  const links = parseLinks(raw ?? "");
  const beta = links.find((l) => l.value === "beta.md");
  const gamma = links.find((l) => l.value === "subdir\\gamma.md");
  if (!beta || beta.kind !== "launchFile") {
    throw new Error(`expected launchFile beta.md, got: ${JSON.stringify(links)}`);
  }
  if (!gamma || gamma.kind !== "launchFile") {
    throw new Error(`expected launchFile subdir\\gamma.md, got: ${JSON.stringify(links)}`);
  }
  console.log(`md-links: OK (${links.length} links)`);
}

if (import.meta.main) {
  await runStandalone(testit);
}