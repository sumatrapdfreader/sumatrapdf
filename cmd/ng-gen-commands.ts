import { main } from "./gen-commands";

export * from "./gen-commands";

if (import.meta.main) {
  main();
}
