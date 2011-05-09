/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "BaseUtil.h"
#include "PEUtil.h"
#include "FileUtil.h"
#include "StrUtil.h"

// Note: temporary
void d(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    ScopedMem<char> buf(str::FmtV(format, args));
    OutputDebugStringA(buf);
    va_end(args);
}

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

/* Layout of PE file, address 0 at the top. Only important fields listed.

IMAGE_DOS_HEADER
  WORD  e_magic
  LONG  e_lfanew - offset of IMAGE_NT_HEADERS

IMAGE_NT_HEADERS
  DWORD Signature;
  IMAGE_FILE_HEADER FileHeader;
    WORD   Machine
    WORD   NumberOfSections
    DWORD  PointerToSymbolTable;
    DWORD  NumberOfSymbols;
    WORD   SizeOfOptionalHeader; 
  IMAGE_OPTIONAL_HEADER32 OptionalHeader
    WORD    Magic;
    DWORD   SizeOfCode;
    DWORD   ImageBase;
    DWORD   SectionAlignment;
    DWORD   FileAlignment;
    DWORD   SizeOfImage;
    DWORD   SizeOfHeaders;
    DWORD   CheckSum;
    DWORD   NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];

IMAGE_SECTION_HEADER
    BYTE    Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
            DWORD   PhysicalAddress;
            DWORD   VirtualSize;
    } Misc;
    DWORD   VirtualAddress;
    DWORD   SizeOfRawData;
    DWORD   PointerToRawData;
    DWORD   PointerToRelocations;
    DWORD   PointerToLinenumbers;
    WORD    NumberOfRelocations;
    WORD    NumberOfLinenumbers;
    DWORD   Characteristics;
*/

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

void DumpDataDir(IMAGE_NT_HEADERS *hdr, int dirIdx, char *dirName)
{
    IMAGE_DATA_DIRECTORY dataDir = hdr->OptionalHeader.DataDirectory[dirIdx];
    d("% 38s: 0x%06x 0x%04x\n", dirName, dataDir.VirtualAddress, dataDir.Size);
}

static void DumpSections(IMAGE_SECTION_HEADER *firstSection, int sectionsCount)
{
    IMAGE_SECTION_HEADER *section = firstSection;
    for (int i=0; i < sectionsCount; i++)
    {
        d("% 6s, phys or size: 0x%06x virt: 0x%06x size raw: 0x%06x ptr to raw: 0x%06x\n", section->Name, section->Misc.VirtualSize, section->VirtualAddress, section->SizeOfRawData, section->PointerToRawData);
        section += 1;
    }
}

static int FileOffsetFromVirtAddr(DWORD virtAddr, IMAGE_SECTION_HEADER *firstSection, int sectionsCount)
{
    IMAGE_SECTION_HEADER *sec = firstSection;
    for (int i = 0; i < sectionsCount; i++)
    {
        if ((virtAddr >= sec->VirtualAddress) && 
            (virtAddr <  sec->VirtualAddress + sec->SizeOfRawData)) {
            return (int)(virtAddr - sec->VirtualAddress + sec->PointerToRawData);
        }
        ++sec;
    }
    return -1;
}

// Write dstFile as srcFile with all binary data resources (RCDATA)
// removed. Used for creating uninstaller from installer.
bool RemoveDataResource(const TCHAR *srcFile, const TCHAR *dstFile)
{
    IMAGE_DATA_DIRECTORY dataDir;

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

    IMAGE_NT_HEADERS *hdr = InFilePtr(IMAGE_NT_HEADERS, dosHdr->e_lfanew);
    if (!IsValidSecondHdr(hdr))
        goto Error;

    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_EXPORT, "IMAGE_DIRECTORY_ENTRY_EXPORT");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_IMPORT, "IMAGE_DIRECTORY_ENTRY_IMPORT");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_RESOURCE, "IMAGE_DIRECTORY_ENTRY_RESOURCE");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_EXCEPTION, "IMAGE_DIRECTORY_ENTRY_EXCEPTION");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_SECURITY, "IMAGE_DIRECTORY_ENTRY_SECURITY");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_BASERELOC, "IMAGE_DIRECTORY_ENTRY_BASERELOC");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_DEBUG, "IMAGE_DIRECTORY_ENTRY_DEBUG");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_ARCHITECTURE, "IMAGE_DIRECTORY_ENTRY_ARCHITECTURE");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_GLOBALPTR, "IMAGE_DIRECTORY_ENTRY_GLOBALPTR");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_TLS, "IMAGE_DIRECTORY_ENTRY_TLS");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, "IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT, "IMAGE_DIRECTORY_ENTRY_BOUND_IMPORT");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_IAT, "IMAGE_DIRECTORY_ENTRY_IAT");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT, "IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT");
    DumpDataDir(hdr, IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR, "IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR");

    int sectionsCount = (int)hdr->FileHeader.NumberOfSections;
    d("NumberOfSections: %d\n", sectionsCount);

    IMAGE_SECTION_HEADER *firstSection = IMAGE_FIRST_SECTION(hdr);
    DumpSections(firstSection, sectionsCount);

    dataDir = hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
    int off = FileOffsetFromVirtAddr(dataDir.VirtualAddress, firstSection, sectionsCount);

    IMAGE_RESOURCE_DIRECTORY *rd = InFilePtr(IMAGE_RESOURCE_DIRECTORY, off);
    d("NumberOfNamedEntries: %d\n", (int)rd->NumberOfNamedEntries);
    d("NumberOfIdEntries : %d\n", (int)rd->NumberOfIdEntries );

    delete mf;
    return true;

Error:
    delete mf;
    return false;
}

