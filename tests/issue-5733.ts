// Regression test for issue #5733: Favorites menu Add/Remove items missing.
//
// Verifies UpdateAppMenu builds the Favorites submenu with a real menu ctx
// so CmdFavoriteAdd/CmdFavoriteDel are not filtered out as "no document".

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { runStandalone } from "./util.ts";

const MENU = join(import.meta.dir, "..", "src", "Menu.cpp");

export async function testit(): Promise<void> {
  const src = readFileSync(MENU, "utf8");
  if (!src.includes("NewBuildMenuCtx(win->CurrentTab(), Point{0, 0})")) {
    throw new Error("issue-5733: Favorites submenu should use NewBuildMenuCtx");
  }
  if (src.includes("BuildMenuFromDef(menuDefFavorites, m, nullptr)")) {
    throw new Error("issue-5733: Favorites submenu must not use nullptr ctx");
  }
  console.log("issue-5733: favorites menu ctx fix present");
}

if (import.meta.main) {
  await runStandalone(testit, { noBuild: true });
}