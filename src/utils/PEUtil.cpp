#include "BaseUtil.h"
#include "PEUtil.h"
#include "FileUtil.h"
#include "StrUtil.h"

class MappedFile {

    HANDLE hFile;
    HANDLE hMapping;

    MappedFile() : hFile(NULL), hMapping(NULL), data(NULL)
    {}

public:
    unsigned char * data;
    size_t size;

    static MappedFile *Create(const TCHAR *filePath);
    ~MappedFile() {
        if (data)
            UnmapViewOfFile(data);
        CloseHandle(hMapping);
        CloseHandle(hFile);
    }
};

MappedFile *MappedFile::Create(const TCHAR *filePath)
{
    MappedFile *mf = new MappedFile();
    mf->size = file::GetSize(filePath);
    if (INVALID_FILE_SIZE == mf->size)
        goto Error;

    mf->hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, (HANDLE)0);
    if (INVALID_HANDLE_VALUE == mf->hFile)
        goto Error;

    mf->hMapping = CreateFileMapping(mf->hFile, NULL, PAGE_READONLY, 0, 0,NULL);
    if (0 == mf->hMapping)
        goto Error;
    mf->data = (unsigned char*)MapViewOfFile(mf->hMapping, FILE_MAP_READ, 0, 0, 0);
    if (NULL == mf->data)
        goto Error;

    return mf;
Error:
    delete mf;
    return NULL;
}

#define InFilePtr(type, offset) (type*)(mf->data + offset)

bool IsValidSecondHdr(IMAGE_NT_HEADERS *hdr)
{
    if (IMAGE_NT_SIGNATURE != hdr->Signature)
        return false;

    // IMAGE_NT_HEADERS are slightly different for 32bit and 64bit,
    // we only support 32bit executables
    if (IMAGE_FILE_MACHINE_I386 != hdr->FileHeader.Machine)
        return false;

    return true;
}

// Write dstFile as srcFile with all binary data resources (RCDATA)
// removed. Used for creating uninstaller from installer.
bool RemoveDataResource(const TCHAR *srcFile, const TCHAR *dstFile)
{
    MappedFile *mf = MappedFile::Create(srcFile);
    if (!mf)
        return false;

    if (mf->size < sizeof(IMAGE_DOS_HEADER))
        goto Error;

    IMAGE_DOS_HEADER* dosHdr = (IMAGE_DOS_HEADER*)mf->data;
    if (IMAGE_DOS_SIGNATURE != dosHdr->e_magic)
        goto Error;

    if (dosHdr->e_lfarlc < 0x40)
        goto Error;

    if ((size_t)dosHdr->e_lfanew > mf->size)
        goto Error;

    IMAGE_NT_HEADERS *secondHdr = InFilePtr(IMAGE_NT_HEADERS, dosHdr->e_lfanew);
    if (!IsValidSecondHdr(secondHdr))
        goto Error;

    delete mf;
    return true;

Error:
    delete mf;
    return false;
}
