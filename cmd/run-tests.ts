import { join, resolve } from "node:path";
import { detectVisualStudio, runLogged } from "./util";

const { msbuildPath } = detectVisualStudio();
const slnPath = join("vs2022", "SumatraPDF.sln");
// Nested under the "tools" solution folder → MSBuild target is tools\test_util
await runLogged(msbuildPath, [
  slnPath,
  String.raw`/t:tools\test_util:Rebuild`,
  `/p:Configuration=Release;Platform=x64`,
  `/m`,
]);

const dir = join("out", "rel64");
await runLogged(resolve(join(dir, "test_util.exe")), [], dir);
