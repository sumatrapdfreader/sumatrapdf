/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}

class ZipCreator {
    ISequentialStream* stream;
    str::Str centraldir;
    size_t bytesWritten;
    size_t fileCount;

    bool WriteData(const void* data, size_t size);
    bool AddFileData(const char* nameUtf8, const void* data, size_t size, u32 dosdate = 0);

  public:
    explicit ZipCreator(const char* zipFilePath);
    explicit ZipCreator(ISequentialStream* stream);
    ~ZipCreator();

    ZipCreator(ZipCreator const&) = delete;
    ZipCreator& operator=(ZipCreator const&) = delete;

    bool AddFile(const char* filePath, const char* nameInZip = nullptr);
    bool AddFileFromDir(const char* filePath, const char* dir);
    bool AddDir(const char* dirPath, bool recursive = false);
    bool Finish();
};

IStream* OpenDirAsZipStream(const char* dirPath, bool recursive = false);

ByteSlice Ungzip(const ByteSlice&);
