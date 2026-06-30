import { $ } from "bun";
import { join } from "node:path";

const premakePath = join("bin", "premake5.exe");
await $`${premakePath} vs2022`;
