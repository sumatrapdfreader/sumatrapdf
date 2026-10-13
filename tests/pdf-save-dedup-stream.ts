// Saving with garbage=3 (deduplication) dropped the data of a stream added in
// this session: the second garbage pass replaced its dictionary through
// pdf_update_object, which started a fresh xref entry without the stream.
import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const PDF = resolve(import.meta.dir, "issue-2423.pdf");

// two pages share the form, so their identical resource dicts get merged
const MAKE = `
var doc = Document.openDocument(scriptArgs[0]);
var page = doc.findPage(0);
var buf = new Buffer();
buf.writeBuffer(page.get("Contents").readStream());
var dict = doc.newDictionary();
dict.put("Type", doc.newName("XObject"));
dict.put("Subtype", doc.newName("Form"));
dict.put("BBox", page.getInheritable("MediaBox"));
dict.put("Resources", page.getInheritable("Resources"));
var form = doc.addStream(buf, dict);
for (var i = 0; i < 2; i++) {
  doc.insertPage(-1, doc.addPage([0, 0, 300, 300], 0, { XObject: { Fm: form } }, "q /Fm Do Q"));
}
doc.save(scriptArgs[1], "garbage=" + scriptArgs[2]);
`;

const CHECK = `
var doc = Document.openDocument(scriptArgs[0]);
var n = doc.countObjects();
for (var i = 1; i < n; i++) {
  var obj = doc.newIndirect(i, 0);
  var r = obj.resolve();
  if (!r || !r.isDictionary() || !r.get("Length")) continue;
  if (!obj.isStream()) throw new Error("object " + i + " has /Length but no stream");
}
print("ok");
`;

function run(args: string[]): string {
  const proc = Bun.spawnSync([EXE, "run", ...args]);
  if (proc.exitCode !== 0) {
    throw new Error(`${args.join(" ")} failed (exit ${proc.exitCode}):\n${proc.stdout}\n${proc.stderr}`);
  }
  return proc.stdout.toString();
}

export async function testit(): Promise<void> {
  const make = tmpPath("pdf-save-dedup-make.js");
  const check = tmpPath("pdf-save-dedup-check.js");
  writeFileSync(make, MAKE);
  writeFileSync(check, CHECK);
  for (const garbage of [2, 3, 4]) {
    const out = tmpPath(`pdf-save-dedup-${garbage}.pdf`);
    run([make, PDF, out, String(garbage)]);
    const res = run([check, out]);
    if (!res.includes("ok")) throw new Error(`garbage=${garbage}: ${res}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
