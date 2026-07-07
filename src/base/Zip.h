/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class ZipCreator {
    str::Builder centraldir;
    str::Builder zipData;
    str::Builder* zipOut;
    Str zipFilePath;
    size_t bytesWritten;
    size_t fileCount;

    bool WriteData(const void* data, size_t size);
    bool AddFileData(Str name, Str data, u32 dosdate = 0);

  public:
    explicit ZipCreator(Str zipFilePath);
    explicit ZipCreator(str::Builder& zipOut);
    ~ZipCreator();

    ZipCreator(ZipCreator const&) = delete;
    ZipCreator& operator=(ZipCreator const&) = delete;

    bool AddFile(Str filePath, Str nameInZip = {});
    bool AddFileFromDir(Str filePath, Str dir);
    bool AddDir(Str dirPath, bool recursive = false);
    bool Finish();
};

Str ZipDirToData(Str dirPath, bool recursive = false);

Str Ungzip(const Str&);
