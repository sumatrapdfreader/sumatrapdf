# SumatraPDF run

**Available in [pre-release 3.7](https://www.sumatrapdfreader.org/prerelease)**

**Usage:** `SumatraPDF run script.js [arguments...]`

`run` executes a [MuPDF](https://mupdf.readthedocs.io/) JavaScript program. It's
the same scripting engine as `mutool run` and gives you programmatic access to
PDF (and other) documents: read and edit pages, annotations, metadata, render to
images, extract text and more.

## Run a script

`SumatraPDF run script.js` runs the program in `script.js`.

A minimal `hello.js`:

```js
print("hello from SumatraPDF run");
```

```
SumatraPDF run hello.js
```

## Open a document

The MuPDF API is available to scripts. For example, `pages.js` prints the number
of pages of a document:

```js
var doc = Document.openDocument(scriptArgs[0]);
print(doc.countPages() + " pages");
```

```
SumatraPDF run pages.js file.pdf
```

## Script arguments

When you run `SumatraPDF run script.js a b c`, the script can read the arguments
that follow the script name:

- `scriptPath` - path of the script (`script.js`)
- `scriptArgs` - array of the arguments after the script (`["a", "b", "c"]`)
- `process.argv` - Node-compatible array of all arguments, including the program
  name and script path

## Interactive mode (REPL)

Run `SumatraPDF run` with no script to start an interactive read-eval-print loop.
It prints a `> ` prompt, evaluates each line you type and prints the result:

```
SumatraPDF run
> 1 + 2
3
> var doc = Document.openDocument("file.pdf")
> doc.countPages()
21
```

Press `Ctrl + Z` then `Enter` (end of input) to quit, or call `quit()`.

## Reading from stdin

`readline()` reads a line from standard input, so scripts can be driven by
piped or typed input (added in 3.7, [issue #5665](https://github.com/sumatrapdfreader/sumatrapdf/issues/5665)).

`echo.js`:

```js
var line = readline();
print("you typed: " + line);
```

```
echo hello | SumatraPDF run echo.js
```

The interactive REPL also reads from stdin, so you can pipe a whole program into
it:

```
echo "print(6*7)" | SumatraPDF run
```

## Built-in functions

In addition to the MuPDF API, these global functions are available:

- `print(...)` - print arguments followed by a newline
- `write(...)` - print arguments without a trailing newline
- `read(filename)` - read a file and return its contents as a string
- `readline()` - read one line from standard input
- `repr(value)` - return a string representation of a value
- `load(filename)` - load and execute another script file
- `gc()` - run the garbage collector
- `quit(code)` - exit the program

## MuPDF JavaScript API

The objects used above (`Document`, `Page`, `Pixmap`, `PDFDocument`, `Buffer`
etc.) are the MuPDF scripting API. `SumatraPDF run` is the same as `mutool run`.
See the upstream documentation:

- [`mutool run`](https://mupdf.readthedocs.io/en/1.27.2/tools/mutool-run.html) - the equivalent MuPDF command-line tool
- [JavaScript cookbook](https://mupdf.readthedocs.io/en/1.27.2/cookbook/javascript/index.html) - example-driven recipes
- [JavaScript API reference](https://mupdf.readthedocs.io/en/1.27.2/reference/javascript/index.html) - full list of classes and methods
