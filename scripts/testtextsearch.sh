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
$exefile "//tester" -search "$pdffile" "rhoncus posuere" out-rhoncus-posuere.log

# Short, then long word
$exefile "//tester" -search "$pdffile" "ut efficitur" out-ut-efficitur.log
# Long, then short word
$exefile "//tester" -search "$pdffile" "euismod ac" out-euismod-ac.log

# Middle of word
$exefile "//tester" -search "$pdffile" spendis out-spendis.log

# Word that doesn't exist in the text
$exefile "//tester" -search "$pdffile" xyzzy out-xyzzy.log

# Words across page boundaries (represented as |)
# This returns no match, but "Morbi" is the last word of page 1 and mattis is the first word of page 2
$exefile "//tester" -search "$pdffile" "morbi mattis" out-morbi-mattis.log
# sit amet | massa (pg 3 | 4)
$exefile "//tester" -search "$pdffile" "sit amet massa" out-sit-amet-massa.log
# convallis | libero nibh (pg 8 | 9)
$exefile "//tester" -search "$pdffile" "convallis libero nibh" out-convallis-libero-nibh.log
# sem eu augue | pellentesque accumsan at (pg 9 | 10)
$exefile "//tester" -search "$pdffile" "sem eu augue pellentesque accumsan at" out-sem-eu-augue-pellentesque-accumsan-at.log

echo Example command to compare results:
echo 'diff -u -I "^Time: " -I "[a-zA-Z ]*in [a-zA-Z/.]*pdf" before.log after.log'
