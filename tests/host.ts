// Which machine the tests/ scripts are running on.
// macOS runs the ng debug ASan app (out/mac/dbg-asan/SumatraPDF).

export const IS_WIN = process.platform === "win32";
export const IS_MAC = process.platform === "darwin";

// Inherited by every child. ASan reads these at process start, so setting
// them here does not change this process. On macOS the nano allocator
// occupies the shadow memory ASan needs.
if (!IS_WIN) {
  const cur = process.env.ASAN_OPTIONS ?? "";
  if (!/(^|:)abort_on_error(=|:|$)/.test(cur)) {
    process.env.ASAN_OPTIONS = cur ? `${cur}:abort_on_error=1` : "abort_on_error=1";
  }
}
if (IS_MAC && process.env.MallocNanoZone === undefined) {
  process.env.MallocNanoZone = "0";
}
