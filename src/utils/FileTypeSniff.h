
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind kindFileZip;
extern Kind kindFileRar;
extern Kind kindFilePng;
extern Kind kindFileJpeg;

// detect file type based on file content
Kind SniffFileType(std::string_view d);
