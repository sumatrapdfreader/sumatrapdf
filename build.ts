import { $} from "bun";
import { unlink } from "node:fs/promises";

await unlink("./out/dbg64/SumatraPDF.exe").catch(() => {});
await $`msbuild .\\vs2022\\SumatraPDF.sln /t:SumatraPDF "/p:Configuration=Debug;Platform=x64" /m`;
