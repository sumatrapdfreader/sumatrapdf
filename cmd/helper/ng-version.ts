// The version of an ng build is the day it was built: yy.mm.dd, plus .n for
// the n-th further build of that day (26.10.03, 26.10.03.1, ...).
// The build passes it to the compiler as defines; see src/ng/Version.h.
import { join } from "node:path";

export const maxRev = 9;

// set by the Linux release workflow: the version build-ng-linux.ts picked,
// so a runner in another time zone doesn't build a different day
export const ngVerEnv = "SUMATRA_NG_VER";
const verRx = /^(\d\d)\.(\d\d)\.(\d\d)(?:\.(\d))?$/;

export type NgVersion = {
  /** 26.10.03.1 */
  ver: string;
  /** 26,10,3,1: FILEVERSION of a Windows version resource */
  rc: string;
  /** "" outside a git checkout */
  gitSha1: string;
};

const root = join(import.meta.dir, "..", "..");

function gitSha1(): string {
  const r = Bun.spawnSync(["git", "rev-parse", "HEAD"], { cwd: root, stdout: "pipe", stderr: "pipe" });
  const s = r.exitCode === 0 ? r.stdout.toString().trim() : "";
  return /^[0-9a-f]{40}$/.test(s) ? s : "";
}

const pad2 = (n: number) => String(n).padStart(2, "0");

const memo = new Map<number, NgVersion>();

export function ngVersion(rev = 0): NgVersion {
  let res = memo.get(rev);
  if (res) return res;
  const fixed = process.env[ngVerEnv];
  if (fixed) {
    const m = verRx.exec(fixed);
    if (!m) throw new Error(`${ngVerEnv}=${fixed}: not yy.mm.dd[.n]`);
    const [yy, mm, dd, r] = m.slice(1).map((s) => Number(s ?? 0)) as [number, number, number, number];
    res = { ver: fixed, rc: `${yy},${mm},${dd},${r}`, gitSha1: gitSha1() };
    memo.set(rev, res);
    return res;
  }
  const now = new Date();
  const [yyyy, mm, dd] = [now.getFullYear(), now.getMonth() + 1, now.getDate()];
  const yy = yyyy % 100;
  res = {
    ver: `${pad2(yy)}.${pad2(mm)}.${pad2(dd)}` + (rev > 0 ? `.${rev}` : ""),
    rc: `${yy},${mm},${dd},${rev}`,
    gitSha1: gitSha1(),
  };
  memo.set(rev, res);
  return res;
}

/** defines for the one source file that turns the version into variables */
export function versionDefines(v: NgVersion): string[] {
  const res = [`SUMATRA_VER=${v.ver}`];
  if (v.gitSha1) res.push(`GIT_COMMIT_ID=${v.gitSha1}`);
  return res;
}
