.. Copyright (C) 2025 Artifex Software, Inc.
.. All Rights Reserved.


.. default-domain:: js


.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


:title:`mutool audit`
==========================================


The `audit` command prints a report of operator and space usage
for a :title:`PDF` file.


.. code-block:: bash

   mutool audit [options] file


.. include:: optional-command-line-note.rst


`[options]`
   Options are as follows:


   `-o` output file
      Use the specified filename for the produced HTML report.


----

`file`
   Input file name. The input must be a :title:`PDF` file.

----


The output report
~~~~~~~~~~~~~~~~~

The report takes the form of an HTML document. The report assumes
a fair degree of familiarity with the :title:`PDF` format.

----

The first section of the file gives figures for the proportions of
the file that are objects vs the overheads for storing those objects,
for both objects inside and outside of ObjStms.

It also lists the size of as a percentage of the whole file.

----

The second section of the file gives figures for the usage of the
different objects within the file.

The classification of objects is simplistic, but works in most
cases.

----

The final section of the file gives figures for the frequency of
use of different graphical operators within operator streams.

These figures should be taken as being indicative, rather
than 100% accurate.





.. External links
