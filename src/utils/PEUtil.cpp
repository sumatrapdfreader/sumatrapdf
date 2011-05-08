#include "BaseUtil.h"
#include "PEUtil.h"
#include "FileUtil.h"
#include "StrUtil.h"
#include "Version.h"

#define TQM(x) _T(_QUOTEME(x))

#define dbg(format, ...) str::DbgOut(format, __VA_ARGS__)

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
/*
#define IMAGE_DIRECTORY_ENTRY_EXPORT          0   // Export Directory
#define IMAGE_DIRECTORY_ENTRY_IMPORT          1   // Import Directory
#define IMAGE_DIRECTORY_ENTRY_RESOURCE        2   // Resource Directory
#define IMAGE_DIRECTORY_ENTRY_EXCEPTION       3   // Exception Directory
#define IMAGE_DIRECTORY_ENTRY_SECURITY        4   // Security Directory
#define IMAGE_DIRECTORY_ENTRY_BASERELOC       5   // Base Relocation Table
#define IMAGE_DIRECTORY_ENTRY_DEBUG           6   // Debug Directory
//      IMAGE_DIRECTORY_ENTRY_COPYRIGHT       7   // (X86 usage)
#define IMAGE_DIRECTORY_ENTRY_ARCHITECTURE    7   // Architecture Specific Data
#define IMAGE_DIRECTORY_ENTRY_GLOBALPTR       8   // RVA of GP
#define IMAGE_DIRECTORY_ENTRY_TLS             9   // TLS Directory
#define IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG    10   // Load Configuration Directory
#define IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT   11   // Bound Import Directory in headers
#define IMAGE_DIRECTORY_ENTRY_IAT            12   // Import Address Table
#define IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT   13   // Delay Load Import Descriptors
#define IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR 14   // COM Runtime descriptor
*/

void DumpDataDir(IMAGE_NT_HEADERS *hdr, int dirIdx, TCHAR *dirName)
{
    IMAGE_DATA_DIRECTORY dataDir = hdr->OptionalHeader.DataDirectory[dirIdx];
    dbg(_T("% 38s: 0x%06x 0x%04x\n"), dirName, dataDir.VirtualAddress, dataDir.Size);
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

    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_EXPORT, _T("IMAGE_DIRECTORY_ENTRY_EXPORT"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_IMPORT, _T("IMAGE_DIRECTORY_ENTRY_IMPORT"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_RESOURCE, _T("IMAGE_DIRECTORY_ENTRY_RESOURCE"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_EXCEPTION, _T("IMAGE_DIRECTORY_ENTRY_EXCEPTION"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_SECURITY, _T("IMAGE_DIRECTORY_ENTRY_SECURITY"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_BASERELOC, _T("IMAGE_DIRECTORY_ENTRY_BASERELOC"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_DEBUG, _T("IMAGE_DIRECTORY_ENTRY_DEBUG"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_ARCHITECTURE, _T("IMAGE_DIRECTORY_ENTRY_ARCHITECTURE"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_GLOBALPTR, _T("IMAGE_DIRECTORY_ENTRY_GLOBALPTR"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_TLS, _T("IMAGE_DIRECTORY_ENTRY_TLS"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, _T("IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT, _T("IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_IAT, _T("IMAGE_DIRECTORY_ENTRY_IAT"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT, _T("IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT"));
    DumpDataDir(secondHdr, IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR, _T("IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR"));

    delete mf;
    return true;

Error:
    delete mf;
    return false;
}

