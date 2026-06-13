// mupdf `run` script for tests/issue-5665.ts: read two lines from stdin and
// echo them back, proving readline() can read piped stdin in the GUI exe.
var a = readline();
var b = readline();
print("got1:[" + a + "]");
print("got2:[" + b + "]");
