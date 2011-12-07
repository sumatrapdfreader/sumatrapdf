/***
*pesect.c - PE image header routines
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Defines routines that query info from a PE image header.  Because
*       one of these queries the current PE image, via the linker-defined
*       variable __ImageBase, this object must be a static-link component
*       of any C Runtime library.
*
*******************************************************************************/

#include <windows.h>

#if defined (_WIN64) && defined (_M_IA64)
#pragma section(".base", long, read)
__declspec(allocate(".base"))
extern IMAGE_DOS_HEADER __ImageBase;
#else  /* defined (_WIN64) && defined (_M_IA64) */
extern IMAGE_DOS_HEADER __ImageBase;
#endif  /* defined (_WIN64) && defined (_M_IA64) */

#pragma optimize("t", on)   // optimize for speed, not space

/***
*BOOL _ValidateImageBase
*
*Purpose:
*       Check if a PE image is located at a potential image base address.
*
*Entry:
*       pImageBase - pointer to potential PE image in memory
*
*Return:
*       TRUE    PE image validated at pImageBase
*       FALSE   PE image not found
*
*******************************************************************************/

BOOL __cdecl _ValidateImageBase(
    PBYTE pImageBase
    )
{
    PIMAGE_DOS_HEADER      pDOSHeader;
    PIMAGE_NT_HEADERS      pNTHeader;
    PIMAGE_OPTIONAL_HEADER pOptHeader;

    pDOSHeader = (PIMAGE_DOS_HEADER)pImageBase;
    if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return FALSE;
    }

    pNTHeader = (PIMAGE_NT_HEADERS)((PBYTE)pDOSHeader + pDOSHeader->e_lfanew);
    if (pNTHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        return FALSE;
    }

    pOptHeader = (PIMAGE_OPTIONAL_HEADER)&pNTHeader->OptionalHeader;
    if (pOptHeader->Magic != IMAGE_NT_OPTIONAL_HDR_MAGIC)
    {
        return FALSE;
    }

    return TRUE;
}

/***
*PIMAGE_SECTION_HEADER _FindPESection
*
*Purpose:
*       Given an RVA (Relative Virtual Address, the offset from the Image Base
*       for a PE image), determine which PE section, if any, includes that RVA.
*
*Entry:
*       pImageBase - pointer to PE image in memory
*       rva - RVA whose enclosing section is to be found
*
*Return:
*       NULL     RVA is not part by any section in the PE image
*       non-NULL Pointer to IMAGE_SECTION_HEADER describing the section holding
*                the RVA
*
*******************************************************************************/

PIMAGE_SECTION_HEADER __cdecl _FindPESection(
    PBYTE     pImageBase,
    DWORD_PTR rva
    )
{
    PIMAGE_NT_HEADERS     pNTHeader;
    PIMAGE_SECTION_HEADER pSection;
    unsigned int          iSection;

    pNTHeader =
        (PIMAGE_NT_HEADERS)
            (pImageBase + ((PIMAGE_DOS_HEADER)pImageBase)->e_lfanew);

    //
    // Find the section holding the desired address.  We make no assumptions
    // here about the sort order of the section descriptors (though they
    // always appear to be sorted by ascending section RVA).
    //
    for (iSection = 0, pSection = IMAGE_FIRST_SECTION(pNTHeader);
         iSection < pNTHeader->FileHeader.NumberOfSections;
         ++iSection, ++pSection)
    {
        if (rva >= pSection->VirtualAddress &&
            rva <  pSection->VirtualAddress + pSection->Misc.VirtualSize)
        {
            //
            // Section found
            //
            return pSection;
        }
    }

    //
    // Section not found
    //
    return NULL;
}

/***
*BOOL _IsNonwritableInCurrentImage
*
*Purpose:
*       Check if an address is located within the current PE image (the one
*       starting at __ImageBase), that it is in a proper section of the image,
*       and that section is not marked writable.  This routine must be
*       statically linked, not imported from the CRT DLL, so the correct
*       __ImageBase is found.
*
*Entry:
*       pTarget - address to check
*
*Return:
*       0        Address is either not in current image, not in a section, or
*                in a writable section.
*       non-0    Address is in a non-writable section of the current image.
*
*******************************************************************************/

BOOL __cdecl _IsNonwritableInCurrentImage(
    PBYTE pTarget
    )
{
    PBYTE                 pImageBase;
    DWORD_PTR             rvaTarget;
    PIMAGE_SECTION_HEADER pSection;

    pImageBase = (PBYTE)&__ImageBase;

    __try {
        //
        // Make sure __ImageBase does address a PE image.  This is likely an
        // unnecessary check, since we should be running from a normal image,
        // but it is fast, this routine is rarely called, and the normal call
        // is for security purposes.  If we don't have a PE image, return
        // failure.
        //
        if (!_ValidateImageBase(pImageBase))
        {
            return FALSE;
        }

        //
        // Convert the targetaddress to a Relative Virtual Address (RVA) within
        // the image, and find the corresponding PE section.  Return failure if
        // the target address is not found within the current image.
        //
        rvaTarget = pTarget - pImageBase;
        pSection = _FindPESection(pImageBase, rvaTarget);
        if (pSection == NULL)
        {
            return FALSE;
        }

        //
        // Check the section characteristics to see if the target address is
        // located within a writable section, returning a failure if yes.
        //
        return (pSection->Characteristics & IMAGE_SCN_MEM_WRITE) == 0;
    }
    __except (GetExceptionCode() == STATUS_ACCESS_VIOLATION)
    {
        //
        // Just return failure if the PE image is corrupted in any way that
        // triggers an AV.
        //
        return FALSE;
    }
}
