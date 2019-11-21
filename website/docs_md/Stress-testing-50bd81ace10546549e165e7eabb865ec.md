# [Documentation](/docs/) : Stress testing

We stress test Sumatra to ensure it doesn't crash. Stress testing on my collection of documents is part of release process.

A simple stress test: run `SumatraPDF.exe -console -n 2 -rand -stress-test ${dir}` It'll scan a given directory, randomize order of files and render every known document type.

Stress test can be customized with the following cmd-line options:

-  `-stress-test foo.pdf 8x` : render only foo.pdf, do it 8 times
-  `-stress-test z:\ ` : render all documents in z:\ directory
-  `-stress-test y:\testdocs *.pdf;*.xps 2x` : render only .pdf and .xps document in y:\testdocs directory, render each document twice
-  `-n ${parallelCount}` : number of documents to test in parallel
-  `-rand` : randomize order of files. Also distributes equally among different file types (helps to test different formats equally as opposed to reading sequentially, which can spend a lot of time testing *pdf files, then a lot of time testing .cbz files etc.)
-  `-console` : show console so that we can see progress