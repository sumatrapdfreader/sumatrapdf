.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js

.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool run`
==========================================



The `run` command executes a :title:`JavaScript` program, which has access to most of the features of the :title:`MuPDF` library. The command supports :title:`ECMAScript` 5 syntax in strict mode. All of the :title:`MuPDF` constructors and functions live in the global object, and the command line arguments are accessible from the global `scriptArgs` object. The name of the script is in the global `scriptPath` variable.

See the :ref:`mutool JavaScript API<mutool_run_javascript_api>` for more.




.. code-block:: bash

   mutool run script.js [ arguments ... ]




.. include:: optional-command-line-note.rst



`script.js`
   The :title:`JavaScript` file which you would like to run.


.. note::

   See the :ref:`mutool JavaScript API<mutool_run_javascript_api>` for more.

----



`[ arguments ... ]`
   If invoked without any arguments, it will drop you into an interactive :title:`REPL` (read-eval-print-loop). To exit this loop type `quit()` (same as pressing *ctrl-C* two times).



:title:`JavaScript` Shell
------------------------------

Several global functions that are common for command line shells are available:

`gc(report)`
   Run the garbage collector to free up memory. Optionally report statistics on the garbage collection.

`load(fileName)`
   Load and execute script in "fileName".

`print(...)`
   Print arguments to `stdout`, separated by spaces and followed by a newline.

`quit()`
   Exit the shell.

`read(fileName)`
   Read the contents of a file and return them as a :title:`UTF-8` decoded string.

`readline()`
   Read one line of input from `stdin` and return it as a string.

`require(module)`
   Load a :title:`JavaScript` module.

`write(...)`
   Print arguments to `stdout`, separated by spaces.



Example scripts
-------------------


Create and edit :title:`PDF` documents
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


**Create PDF document from scratch using only low level functions**


.. literalinclude:: ../examples/pdf-create-lowlevel.js
   :caption: docs/examples/pdf-create-lowlevel.js
   :language: javascript

----

**Create PDF document from scratch, using helper functions**


.. literalinclude:: ../examples/pdf-create.js
   :caption: docs/examples/pdf-create.js
   :language: javascript

----

**Merge pages from multiple PDF documents into one PDF file**

.. literalinclude:: ../examples/pdf-merge.js
   :caption: docs/examples/pdf-merge.js
   :language: javascript


Graphics and the device interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Draw all pages in a document to PNG files**

.. literalinclude:: ../examples/draw-document.js
   :caption: docs/examples/draw-document.js
   :language: javascript

----

**Use device API to draw graphics and save as a PNG file**

.. literalinclude:: ../examples/draw-device.js
   :caption: docs/examples/draw-device.js
   :language: javascript


----

**Implement a device in JavaScript**

.. literalinclude:: ../examples/trace-device.js
   :caption: docs/examples/trace-device.js
   :language: javascript



Advanced examples
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


**Create a PDF from rendered page thumbnails**


.. literalinclude:: ../examples/create-thumbnail.js
   :caption: docs/examples/create-thumbnail.js
   :language: javascript





.. include:: footer.rst



.. External links
