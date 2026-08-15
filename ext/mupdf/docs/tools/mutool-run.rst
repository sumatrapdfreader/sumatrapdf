mutool run
==========================================

The ``run`` command executes a Javascript program, which has access to most of the features of the MuPDF library.
The command supports ECMAScript 5 syntax in strict mode.

.. code-block:: bash

	mutool run script.js [ arguments ... ]

``script.js``
	The Javascript file which you would like to run.

``[ arguments ... ]``
	Any extra arguments you want to pass to the script.
	These are available in the global ``scriptArgs`` array.

If invoked without any arguments, it will drop you into an interactive REPL (read-eval-print-loop).
To exit call the ``quit()`` function or press *Ctrl-D*.

Environment
-----------

Command line arguments are accessible from the global ``scriptArgs`` array.
The name of the script is in the global ``scriptPath`` variable.

The following functions are only available in the mutool run shell:

``require(module)``
	Load a Javascript module.

``repr(value)``
	Convert any Javascript value to a string.

``gc(report)``
	Run the garbage collector to free up memory. Optionally report statistics on the garbage collection.

``load(fileName)``
	Load and execute script in "fileName".

``print(...)``
	Print arguments to ``stdout``, separated by spaces and followed by a newline.

``quit()``
	Exit the shell.

``read(fileName)``
	Read the contents of a file and return them as a UTF-8 decoded string.

``readFile(fileName)``
	Read the contents from a file and return them as a Buffer.

``readline()``
	Read one line of input from standard input and return it as a string.

``write(...)``
	Print arguments to ``stdout``, separated by spaces.

Node Compatibility
------------------

For compatibility with Node the following objects are provided:

``process.argv``
	The command line arguments.

``fs.readFileSync``
	Read a file into a buffer.

``fs.writeFileSync``
	Write a buffer to file.

The following ESM import statements are ignored:

.. code-block::

	import * as fs from "fs"
	import * as mupdf from "mupdf"

If you only use these functions and otherwise stick to ES5 syntax and CommonJS
imports, your scripts should be able to run both with mutool run and Node.
