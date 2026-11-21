/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// `SumatraPDF cli <command>`: read and convert documents from a terminal, a
// script or an AI agent. Every command takes --json, has --help with examples
// and never changes the files it is given. See CliMain() for the exit codes.

// argv[0] is the command (argv after "cli"). Returns the process exit code.
int CliMain(int argc, char** argv);
