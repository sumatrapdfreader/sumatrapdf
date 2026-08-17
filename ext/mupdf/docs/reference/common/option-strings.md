# Option Strings

Many functions in MuPDF take a generic set of options to customize behavior in
an extensible manner.

- PDF writer options
- Generic document writer options
- Structured text extraction options
- etc.

These options strings may use any one of the three supported syntaxes to
represent key-value pairs.

Boolean values can be represented in several equivalent ways:

- true, yes, on, enable, 1
- false, no, off, disable, 0

An option with an empty value counts as true.

## Comma-separated values

Classic syntax with comma-separated key-value pairs.

Values may be enclosed in double quotes to allow embedding commas
and equal signs. To represent a double quote within a quoted value,
use two double quotes in a row.

	compress,encrypt=aes-256,owner-password="Hello, ""world""!"

## URL query string

If the option string starts with a question mark, it will be parsed using URL query string syntax.

	?compress=true&encrypt=aes-256&owner-password=Hello,%20%22world%22%21

Encode special characters using `%HH` hexadecimal escapes.

## JSON

The string can also be in a subset of JSON -- a single JSON object containing only booleans,
numbers, strings, and arrays of numbers.

	{"compress":true,"encrypt":"aes-256","owner-password":"Hello, \"world\"!"}

Escape double quotes and backslashes with a backslash.
