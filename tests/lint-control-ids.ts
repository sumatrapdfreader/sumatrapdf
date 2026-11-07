// Lint: -dbg-control command ids must mean the same command everywhere.
//
// The wire id of a ControlCmd is defined four times: orig's and ng's enums
// (src/SumatraControl.cpp, src/ng/SumatraControl.cpp), the test client
// (tests/control.ts) and cmd/ng-dbg-control.ts. ng once gave orig's 101 / 102
// (StartPerfLog / StopPerfLog) to its own commands, so a test asking for one ran
// the other with no error. A command may exist in only some of them; when two
// define the same name or the same id, they must agree.
//
// Run standalone: bun tests/lint-control-ids.ts  (also runs in run-almost-all.ts)

import { readFileSync } from "node:fs";
import { join } from "node:path";

const root = join(import.meta.dir, "..");

// Name = id lines of an enum or object literal; implicit enum ids count up
function parseIds(relPath: string, start: RegExp, sep: "=" | ":"): Map<string, number> {
  const text = readFileSync(join(root, relPath), "utf-8");
  const m = start.exec(text);
  if (!m) {
    throw new Error(`lint-control-ids: no command table in ${relPath}`);
  }
  const body = text.slice(m.index + m[0].length, text.indexOf("}", m.index));
  const re = sep === "=" ? /^\s*([A-Za-z_]\w*)\s*(?:=\s*(\d+))?\s*,/ : /^\s*([A-Za-z_]\w*)\s*:\s*(\d+)\s*,/;
  const ids = new Map<string, number>();
  let next = 0;
  for (const raw of body.split("\n")) {
    const mm = re.exec(raw.replace(/\/\/.*$/, ""));
    if (!mm) {
      continue;
    }
    const id = mm[2] !== undefined ? parseInt(mm[2], 10) : next;
    ids.set(mm[1]!, id);
    next = id + 1;
  }
  return ids;
}

export async function testit(): Promise<void> {
  const tables: [string, Map<string, number>][] = [
    ["orig", parseIds("src/SumatraControl.cpp", /enum class ControlCmd[^{]*\{/, "=")],
    ["ng", parseIds("src/ng/SumatraControl.cpp", /enum class ControlCmd[^{]*\{/, "=")],
    ["tests/control.ts", parseIds("tests/control.ts", /export enum ControlCommand\s*\{/, "=")],
    ["cmd/ng-dbg-control.ts", parseIds("cmd/ng-dbg-control.ts", /controlCommands[^{]*\{/, ":")],
  ];
  const problems: string[] = [];
  for (let i = 0; i < tables.length; i++) {
    for (let j = i + 1; j < tables.length; j++) {
      const [an, a] = tables[i]!;
      const [bn, b] = tables[j]!;
      const bById = new Map<number, string>();
      for (const [name, id] of b) {
        bById.set(id, name);
      }
      for (const [name, id] of a) {
        const bId = b.get(name);
        if (bId !== undefined && bId !== id) {
          problems.push(`${name}: ${an}=${id} ${bn}=${bId}`);
        }
        const bName = bById.get(id);
        if (bName !== undefined && bName !== name && !a.has(bName) && !b.has(name)) {
          problems.push(`id ${id}: ${an}=${name} ${bn}=${bName}`);
        }
      }
    }
  }
  if (problems.length > 0) {
    throw new Error("lint-control-ids: control command ids disagree:\n" + problems.join("\n"));
  }
  console.log("PASS: control command ids agree");
}

if (import.meta.main) {
  // pure static lint - no app build needed
  try {
    await testit();
  } catch (e) {
    console.error(`❌ ${(e as Error)?.message ?? e}`);
    process.exit(1);
  }
}
