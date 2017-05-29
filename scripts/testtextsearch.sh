#!/bin/bash
exefile="$(dirname "$0")/../dbg/SumatraPDF.exe"
pdffile="$(dirname "$0")/loremipsum.pdf"

# Single letter (lower case)
$exefile "//tester" -search "$pdffile" s out-s-lower.log
$exefile "//tester" -search "$pdffile" S out-s-upper.log

# Single word
$exefile "//tester" -search "$pdffile" suspendisse out-suspendisse.log

# Two words
$exefile "//tester" -search "$pdffile" "fermentum ultricies" out-fermentum-ultricies.log
# BUG? $exefile "//tester" -search "$pdffile" "morbi mattis" out-morbi-mattis.log
# This returns no match, but "Morbi" is the last word of page 1 and mattis is the first word of page 2
$exefile "//tester" -search "$pdffile" "rhoncus posuere" out-rhoncus-posuere.log
# Short, then long word
$exefile "//tester" -search "$pdffile" "ut efficitur" out-ut-efficitur.log
# Long, then short word
$exefile "//tester" -search "$pdffile" "euismod ac" out-euismod-ac.log

# Middle of word
$exefile "//tester" -search "$pdffile" spendis out-spendis.log

# Word that doesn't exist in the text
$exefile "//tester" -search "$pdffile" xyzzy out-xyzzy.log
