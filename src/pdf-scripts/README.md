# PDF script examples

Bundled by `cmd/gen-pdf-scripts.ts` into `src/shared/PDFScriptExamples.inc`.
Run the generator after editing a script. Both apps compile that snapshot.

Examples are adapted from the SumatraPDF JavaScript examples page and
MuPDF's `docs/examples/` (AGPL-3.0). Use MuJS syntax: no ES module imports
or trailing commas in function calls.

Arguments are input at `scriptArgs[0]`, output at `scriptArgs[1]`, then any
extra inputs. `pdf-dejpx.js` is MuPDF's JPEG 2000 replacement example.
