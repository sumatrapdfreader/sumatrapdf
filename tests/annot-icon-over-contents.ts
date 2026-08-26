// Combined into tests/annot-editor.ts (one Sumatra session).
import { testit } from "./annot-editor.ts";
import { runStandalone } from "./util.ts";

export { testit };

if (import.meta.main) {
  await runStandalone(testit);
}
