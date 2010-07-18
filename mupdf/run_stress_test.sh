#!/bin/sh
# Run benchmarks on my collection of pdf 1000+ pdf files
# to see if there's an increase in crashes/failures

#test/benchdir.py obj-rel/pdfbench /Volumes/HD500GB/fitz-test-pdfs/
test/benchdir.py obj-rel/pdfbench ~/fitz-test-pdfs
