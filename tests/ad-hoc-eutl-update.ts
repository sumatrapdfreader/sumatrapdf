// Downloads the real EU Trusted Lists into a throw-away app data dir and checks
// they parse: needs the network and takes a minute, so it isn't in the suites.

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, writeAppdata } from "./util.ts";

const MIN_CERTS = 500;
const MIN_GRANTED_PERIODS = 500;
const UPDATE_TIMEOUT_MS = 300_000;
const SAMPLES = ["test_sign_PAdES_B-T.pdf", "test_sign_PAdES_B-LTA.pdf"];

export async function testit(): Promise<void> {
  const appdata = writeAppdata("ad-hoc-eutl", "RestoreSession = false\nCheckForUpdates = false\n");
  const pdf = join(ROOT, "tests", "issue-5581-data", SAMPLES[0]!);
  const raw = await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestEutlUpdate, [], UPDATE_TIMEOUT_MS);
      if (res[0] !== 0) {
        throw new Error(`ad-hoc-eutl-update: ${res[1]}`);
      }
      console.log(String(res[1]));
      const sigs = await client.request(ControlCommand.TestDocumentSignatures);
      return String(sigs[1] ?? "");
    },
    ["-appdata", appdata, pdf],
  );
  console.log(raw);
  const lines = readFileSync(join(appdata, "eutl-services.txt"), "utf8").split("\n");
  const nCerts = lines.filter((l) => l.startsWith("C ")).length;
  const granted = lines.filter((l) => l.startsWith("S "));
  const nTsa = granted.filter((l) => l.split(" ")[2] === "TSA").length;
  const nCurrent = granted.filter((l) => l.split(" ")[4] === "0").length;
  console.log(`certs ${nCerts}, granted periods ${granted.length}, TSA ${nTsa}, still in effect ${nCurrent}`);
  if (nCerts < MIN_CERTS || granted.length < MIN_GRANTED_PERIODS || nTsa === 0 || nCurrent === 0) {
    throw new Error("ad-hoc-eutl-update: the lists look too small");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
