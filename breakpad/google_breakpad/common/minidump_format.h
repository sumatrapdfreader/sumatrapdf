/* Copyright (c) 2006, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* minidump_format.h: A cross-platform reimplementation of minidump-related
 * portions of DbgHelp.h from the Windows Platform SDK.
 *
 * (This is C99 source, please don't corrupt it with C++.)
 *
 * This file contains the necessary definitions to read minidump files
 * produced on win32/x86.  These files may be read on any platform provided
 * that the alignments of these structures on the processing system are
 * identical to the alignments of these structures on the producing system.
 * For this reason, precise-sized types are used.  The structures defined
 * by this file have been laid out to minimize alignment problems by ensuring
 * ensuring that all members are aligned on their natural boundaries.  In
 * In some cases, tail-padding may be significant when different ABIs specify
 * different tail-padding behaviors.  To avoid problems when reading or
 * writing affected structures, MD_*_SIZE macros are provided where needed,
 * containing the useful size of the structures without padding.
 *
 * Structures that are defined by Microsoft to contain a zero-length array
 * are instead defined here to contain an array with one element, as
 * zero-length arrays are forbidden by standard C and C++.  In these cases,
 * *_minsize constants are provided to be used in place of sizeof.  For a
 * cleaner interface to these sizes when using C++, see minidump_size.h.
 *
 * These structures are also sufficient to populate minidump files.
 *
 * These definitions may be extended to support handling minidump files
 * for other CPUs and other operating systems.
 *
 * Because precise data type sizes are crucial for this implementation to
 * function properly and portably in terms of interoperability with minidumps
 * produced by DbgHelp on Windows, a set of primitive types with known sizes
 * are used as the basis of each structure defined by this file.  DbgHelp
 * on Windows is assumed to be the reference implementation; this file
 * seeks to provide a cross-platform compatible implementation.  To avoid
 * collisions with the types and values defined and used by DbgHelp in the
 * event that this implementation is used on Windows, each type and value
 * defined here is given a new name, beginning with "MD".  Names of the
 * equivalent types and values in the Windows Platform SDK are given in
 * comments.
 *
 * Author: Mark Mentovai */
 

#ifndef GOOGLE_BREAKPAD_COMMON_MINIDUMP_FORMAT_H__
#define GOOGLE_BREAKPAD_COMMON_MINIDUMP_FORMAT_H__

#include <stddef.h>

#include "google_breakpad/common/breakpad_types.h"


#if defined(_MSC_VER)
/* Disable "zero-sized array in struct/union" warnings when compiling in
 * MSVC.  DbgHelp.h does this too. */
#pragma warning(push)
#pragma warning(disable:4200)
#endif  /* _MSC_VER */


/*
 * guiddef.h
 */


typedef struct {
  u_int32_t data1;
  u_int16_t data2;
  u_int16_t data3;
  u_int8_t  data4[8];
} MDGUID;  /* GUID */


/*
 * WinNT.h
 */


#define MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE 80
     /* SIZE_OF_80387_REGISTERS */

typedef struct {
  u_int32_t control_word;
  u_int32_t status_word;
  u_int32_t tag_word;
  u_int32_t error_offset;
  u_int32_t error_selector;
  u_int32_t data_offset;
  u_int32_t data_selector;

  /* register_area contains eight 80-bit (x87 "long double") quantities for
   * floating-point registers %st0 (%mm0) through %st7 (%mm7). */
  u_int8_t  register_area[MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE];
  u_int32_t cr0_npx_state;
} MDFloatingSaveAreaX86;  /* FLOATING_SAVE_AREA */


#define MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE 512
     /* MAXIMUM_SUPPORTED_EXTENSION */

typedef struct {
  /* The next field determines the layout of the structure, and which parts
   * of it are populated */
  u_int32_t             context_flags;

  /* The next 6 registers are included with MD_CONTEXT_X86_DEBUG_REGISTERS */
  u_int32_t             dr0;
  u_int32_t             dr1;
  u_int32_t             dr2;
  u_int32_t             dr3;
  u_int32_t             dr6;
  u_int32_t             dr7;

  /* The next field is included with MD_CONTEXT_X86_FLOATING_POINT */
  MDFloatingSaveAreaX86 float_save;

  /* The next 4 registers are included with MD_CONTEXT_X86_SEGMENTS */
  u_int32_t             gs; 
  u_int32_t             fs;
  u_int32_t             es;
  u_int32_t             ds;
  /* The next 6 registers are included with MD_CONTEXT_X86_INTEGER */
  u_int32_t             edi;
  u_int32_t             esi;
  u_int32_t             ebx;
  u_int32_t             edx;
  u_int32_t             ecx;
  u_int32_t             eax;

  /* The next 6 registers are included with MD_CONTEXT_X86_CONTROL */
  u_int32_t             ebp;
  u_int32_t             eip;
  u_int32_t             cs;      /* WinNT.h says "must be sanitized" */
  u_int32_t             eflags;  /* WinNT.h says "must be sanitized" */
  u_int32_t             esp;
  u_int32_t             ss;

  /* The next field is included with MD_CONTEXT_X86_EXTENDED_REGISTERS.
   * It contains vector (MMX/SSE) registers.  It it laid out in the
   * format used by the fxsave and fsrstor instructions, so it includes
   * a copy of the x87 floating-point registers as well.  See FXSAVE in
   * "Intel Architecture Software Developer's Manual, Volume 2." */
  u_int8_t              extended_registers[
                         MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE];
} MDRawContextX86;  /* CONTEXT */

/* For (MDRawContextX86).context_flags.  These values indicate the type of
 * context stored in the structure.  The high 26 bits identify the CPU, the
 * low 6 bits identify the type of context saved. */
#define MD_CONTEXT_X86                    0x00010000
     /* CONTEXT_i386, CONTEXT_i486: identifies CPU */
#define MD_CONTEXT_X86_CONTROL            (MD_CONTEXT_X86 | 0x00000001)
     /* CONTEXT_CONTROL */
#define MD_CONTEXT_X86_INTEGER            (MD_CONTEXT_X86 | 0x00000002)
     /* CONTEXT_INTEGER */
#define MD_CONTEXT_X86_SEGMENTS           (MD_CONTEXT_X86 | 0x00000004)
     /* CONTEXT_SEGMENTS */
#define MD_CONTEXT_X86_FLOATING_POINT     (MD_CONTEXT_X86 | 0x00000008)
     /* CONTEXT_FLOATING_POINT */
#define MD_CONTEXT_X86_DEBUG_REGISTERS    (MD_CONTEXT_X86 | 0x00000010)
     /* CONTEXT_DEBUG_REGISTERS */
#define MD_CONTEXT_X86_EXTENDED_REGISTERS (MD_CONTEXT_X86 | 0x00000020)
     /* CONTEXT_EXTENDED_REGISTERS */

#define MD_CONTEXT_X86_FULL              (MD_CONTEXT_X86_CONTROL | \
                                          MD_CONTEXT_X86_INTEGER | \
                                          MD_CONTEXT_X86_SEGMENTS)
     /* CONTEXT_FULL */

#define MD_CONTEXT_X86_ALL               (MD_CONTEXT_X86_FULL | \
                                          MD_CONTEXT_X86_FLOATING_POINT | \
                                          MD_CONTEXT_X86_DEBUG_REGISTERS | \
                                          MD_CONTEXT_X86_EXTENDED_REGISTERS)
     /* CONTEXT_ALL */

/* Non-x86 CPU identifiers found in the high 26 bits of
 * (MDRawContext*).context_flags.  These aren't used by Breakpad, but are
 * defined here for reference, to avoid assigning values that conflict
 * (although some values already conflict). */
#define MD_CONTEXT_IA64  0x00080000  /* CONTEXT_IA64 */
#define MD_CONTEXT_AMD64 0x00100000  /* CONTEXT_AMD64 */
/* Additional values from winnt.h in the Windows CE 5.0 SDK: */
#define MD_CONTEXT_SHX   0x000000c0  /* CONTEXT_SH4 (Super-H, includes SH3) */
#define MD_CONTEXT_ARM   0x00000040  /* CONTEXT_ARM (0x40 bit set in SHx?) */
#define MD_CONTEXT_MIPS  0x00010000  /* CONTEXT_R4000 (same value as x86?) */
#define MD_CONTEXT_ALPHA 0x00020000  /* CONTEXT_ALPHA */

#define MD_CONTEXT_CPU_MASK 0xffffffc0


/*
 * Breakpad minidump extension for PowerPC support.  Based on Darwin/Mac OS X'
 * mach/ppc/_types.h
 */


/* This is a base type for MDRawContextX86 and MDRawContextPPC.  This
 * structure should never be allocated directly.  The actual structure type
 * can be determined by examining the context_flags field. */
typedef struct {
  u_int32_t context_flags;
} MDRawContextBase;


#define MD_FLOATINGSAVEAREA_PPC_FPR_COUNT 32

typedef struct {
  /* fpregs is a double[32] in mach/ppc/_types.h, but a u_int64_t is used
   * here for precise sizing. */
  u_int64_t fpregs[MD_FLOATINGSAVEAREA_PPC_FPR_COUNT];
  u_int32_t fpscr_pad;
  u_int32_t fpscr;      /* Status/control */
} MDFloatingSaveAreaPPC;  /* Based on ppc_float_state */


#define MD_VECTORSAVEAREA_PPC_VR_COUNT 32

typedef struct {
  /* Vector registers (including vscr) are 128 bits, but mach/ppc/_types.h
   * exposes them as four 32-bit quantities. */
  u_int128_t save_vr[MD_VECTORSAVEAREA_PPC_VR_COUNT];
  u_int128_t save_vscr;  /* Status/control */
  u_int32_t  save_pad5[4];
  u_int32_t  save_vrvalid;  /* Identifies which vector registers are saved */
  u_int32_t  save_pad6[7];
} MDVectorSaveAreaPPC;  /* ppc_vector_state */


#define MD_CONTEXT_PPC_GPR_COUNT 32

typedef struct {
  /* context_flags is not present in ppc_thread_state, but it aids
   * identification of MDRawContextPPC among other raw context types,
   * and it guarantees alignment when we get to float_save. */
  u_int32_t             context_flags;

  u_int32_t             srr0;    /* Machine status save/restore: stores pc
                                  * (instruction) */
  u_int32_t             srr1;    /* Machine status save/restore: stores msr
                                  * (ps, program/machine state) */
  /* ppc_thread_state contains 32 fields, r0 .. r31.  Here, an array is
   * used for brevity. */
  u_int32_t             gpr[MD_CONTEXT_PPC_GPR_COUNT];
  u_int32_t             cr;      /* Condition */
  u_int32_t             xer;     /* Integer (fiXed-point) exception */
  u_int32_t             lr;      /* Link */
  u_int32_t             ctr;     /* Count */
  u_int32_t             mq;      /* Multiply/Quotient (PPC 601, POWER only) */
  u_int32_t             vrsave;  /* Vector save */

  /* float_save and vector_save aren't present in ppc_thread_state, but
   * are represented in separate structures that still define a thread's
   * context. */
  MDFloatingSaveAreaPPC float_save;
  MDVectorSaveAreaPPC   vector_save;
} MDRawContextPPC;  /* Based on ppc_thread_state */

/* For (MDRawContextPPC).context_flags.  These values indicate the type of
 * context stored in the structure.  MD_CONTEXT_PPC is Breakpad-defined.  Its
 * value was chosen to avoid likely conflicts with MD_CONTEXT_* for other
 * CPUs. */
#define MD_CONTEXT_PPC                0x20000000
#define MD_CONTEXT_PPC_BASE           (MD_CONTEXT_PPC | 0x00000001)
#define MD_CONTEXT_PPC_FLOATING_POINT (MD_CONTEXT_PPC | 0x00000008)
#define MD_CONTEXT_PPC_VECTOR         (MD_CONTEXT_PPC | 0x00000020)

#define MD_CONTEXT_PPC_FULL           MD_CONTEXT_PPC_BASE
#define MD_CONTEXT_PPC_ALL            (MD_CONTEXT_PPC_FULL | \
                                       MD_CONTEXT_PPC_FLOATING_POINT | \
                                       MD_CONTEXT_PPC_VECTOR)


/*
 * WinVer.h
 */


typedef struct {
  u_int32_t signature;
  u_int32_t struct_version;
  u_int32_t file_version_hi;
  u_int32_t file_version_lo;
  u_int32_t product_version_hi;
  u_int32_t product_version_lo;
  u_int32_t file_flags_mask;    /* Identifies valid bits in fileFlags */
  u_int32_t file_flags;
  u_int32_t file_os;
  u_int32_t file_type;
  u_int32_t file_subtype;
  u_int32_t file_date_hi;
  u_int32_t file_date_lo;
} MDVSFixedFileInfo;  /* VS_FIXEDFILEINFO */

/* For (MDVSFixedFileInfo).signature */
#define MD_VSFIXEDFILEINFO_SIGNATURE 0xfeef04bd
     /* VS_FFI_SIGNATURE */

/* For (MDVSFixedFileInfo).version */
#define MD_VSFIXEDFILEINFO_VERSION 0x00010000
     /* VS_FFI_STRUCVERSION */

/* For (MDVSFixedFileInfo).file_flags_mask and
 * (MDVSFixedFileInfo).file_flags */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_DEBUG        0x00000001
     /* VS_FF_DEBUG */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PRERELEASE   0x00000002
     /* VS_FF_PRERELEASE */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PATCHED      0x00000004
     /* VS_FF_PATCHED */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_PRIVATEBUILD 0x00000008
     /* VS_FF_PRIVATEBUILD */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_INFOINFERRED 0x00000010
     /* VS_FF_INFOINFERRED */
#define MD_VSFIXEDFILEINFO_FILE_FLAGS_SPECIALBUILD 0x00000020
     /* VS_FF_SPECIALBUILD */

/* For (MDVSFixedFileInfo).file_os: high 16 bits */
#define MD_VSFIXEDFILEINFO_FILE_OS_UNKNOWN    0          /* VOS_UNKNOWN */
#define MD_VSFIXEDFILEINFO_FILE_OS_DOS        (1 << 16)  /* VOS_DOS */
#define MD_VSFIXEDFILEINFO_FILE_OS_OS216      (2 << 16)  /* VOS_OS216 */
#define MD_VSFIXEDFILEINFO_FILE_OS_OS232      (3 << 16)  /* VOS_OS232 */
#define MD_VSFIXEDFILEINFO_FILE_OS_NT         (4 << 16)  /* VOS_NT */
#define MD_VSFIXEDFILEINFO_FILE_OS_WINCE      (5 << 16)  /* VOS_WINCE */
/* Low 16 bits */
#define MD_VSFIXEDFILEINFO_FILE_OS__BASE      0          /* VOS__BASE */
#define MD_VSFIXEDFILEINFO_FILE_OS__WINDOWS16 1          /* VOS__WINDOWS16 */
#define MD_VSFIXEDFILEINFO_FILE_OS__PM16      2          /* VOS__PM16 */
#define MD_VSFIXEDFILEINFO_FILE_OS__PM32      3          /* VOS__PM32 */
#define MD_VSFIXEDFILEINFO_FILE_OS__WINDOWS32 4          /* VOS__WINDOWS32 */

/* For (MDVSFixedFileInfo).file_type */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_UNKNOWN    0  /* VFT_UNKNOWN */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_APP        1  /* VFT_APP */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_DLL        2  /* VFT_DLL */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_DRV        3  /* VFT_DLL */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_FONT       4  /* VFT_FONT */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_VXD        5  /* VFT_VXD */
#define MD_VSFIXEDFILEINFO_FILE_TYPE_STATIC_LIB 7  /* VFT_STATIC_LIB */

/* For (MDVSFixedFileInfo).file_subtype */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_UNKNOWN                0
     /* VFT2_UNKNOWN */
/* with file_type = MD_VSFIXEDFILEINFO_FILETYPE_DRV */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_PRINTER            1
     /* VFT2_DRV_PRINTER */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_KEYBOARD           2
     /* VFT2_DRV_KEYBOARD */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_LANGUAGE           3
     /* VFT2_DRV_LANGUAGE */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_DISPLAY            4
     /* VFT2_DRV_DISPLAY */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_MOUSE              5
     /* VFT2_DRV_MOUSE */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_NETWORK            6
     /* VFT2_DRV_NETWORK */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_SYSTEM             7
     /* VFT2_DRV_SYSTEM */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_INSTALLABLE        8
     /* VFT2_DRV_INSTALLABLE */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_SOUND              9
     /* VFT2_DRV_SOUND */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_COMM              10
     /* VFT2_DRV_COMM */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_INPUTMETHOD       11
     /* VFT2_DRV_INPUTMETHOD */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_DRV_VERSIONED_PRINTER 12
     /* VFT2_DRV_VERSIONED_PRINTER */
/* with file_type = MD_VSFIXEDFILEINFO_FILETYPE_FONT */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_RASTER            1
     /* VFT2_FONT_RASTER */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_VECTOR            2
     /* VFT2_FONT_VECTOR */
#define MD_VSFIXEDFILEINFO_FILE_SUBTYPE_FONT_TRUETYPE          3
     /* VFT2_FONT_TRUETYPE */


/*
 * DbgHelp.h
 */


/* An MDRVA is an offset into the minidump file.  The beginning of the
 * MDRawHeader is at offset 0. */
typedef u_int32_t MDRVA;  /* RVA */


typedef struct {
  u_int32_t data_size;
  MDRVA     rva;
} MDLocationDescriptor;  /* MINIDUMP_LOCATION_DESCRIPTOR */


typedef struct {
  /* The base address of the memory range on the host that produced the
   * minidump. */
  u_int64_t            start_of_memory_range;

  MDLocationDescriptor memory;
} MDMemoryDescriptor;  /* MINIDUMP_MEMORY_DESCRIPTOR */


typedef struct {
  u_int32_t signature;
  u_int32_t version;
  u_int32_t stream_count;
  MDRVA     stream_directory_rva;  /* A |stream_count|-sized array of
                                    * MDRawDirectory structures. */
  u_int32_t checksum;              /* Can be 0.  In fact, that's all that's
                                    * been found in minidump files. */
  u_int32_t time_date_stamp;       /* time_t */
  u_int64_t flags;
} MDRawHeader;  /* MINIDUMP_HEADER */

/* For (MDRawHeader).signature and (MDRawHeader).version.  Note that only the
 * low 16 bits of (MDRawHeader).version are MD_HEADER_VERSION.  Per the
 * documentation, the high 16 bits are implementation-specific. */
#define MD_HEADER_SIGNATURE 0x504d444d /* 'PMDM' */
     /* MINIDUMP_SIGNATURE */
#define MD_HEADER_VERSION   0x0000a793 /* 42899 */
     /* MINIDUMP_VERSION */

/* For (MDRawHeader).flags: */
typedef enum {
  /* MD_NORMAL is the standard type of minidump.  It includes full
   * streams for the thread list, module list, exception, system info,
   * and miscellaneous info.  A memory list stream is also present,
   * pointing to the same stack memory contained in the thread list,
   * as well as a 256-byte region around the instruction address that
   * was executing when the exception occurred.  Stack memory is from
   * 4 bytes below a thread's stack pointer up to the top of the
   * memory region encompassing the stack. */
  MD_NORMAL                            = 0x00000000,
  MD_WITH_DATA_SEGS                    = 0x00000001,
  MD_WITH_FULL_MEMORY                  = 0x00000002,
  MD_WITH_HANDLE_DATA                  = 0x00000004,
  MD_FILTER_MEMORY                     = 0x00000008,
  MD_SCAN_MEMORY                       = 0x00000010,
  MD_WITH_UNLOADED_MODULES             = 0x00000020,
  MD_WITH_INDIRECTLY_REFERENCED_MEMORY = 0x00000040,
  MD_FILTER_MODULE_PATHS               = 0x00000080,
  MD_WITH_PROCESS_THREAD_DATA          = 0x00000100,
  MD_WITH_PRIVATE_READ_WRITE_MEMORY    = 0x00000200,
  MD_WITHOUT_OPTIONAL_DATA             = 0x00000400,
  MD_WITH_FULL_MEMORY_INFO             = 0x00000800,
  MD_WITH_THREAD_INFO                  = 0x00001000,
  MD_WITH_CODE_SEGS                    = 0x00002000
} MDType;  /* MINIDUMP_TYPE */


typedef struct {
  u_int32_t            stream_type;
  MDLocationDescriptor location;
} MDRawDirectory;  /* MINIDUMP_DIRECTORY */

/* For (MDRawDirectory).stream_type */
typedef enum {
  MD_UNUSED_STREAM               =  0,
  MD_RESERVED_STREAM_0           =  1,
  MD_RESERVED_STREAM_1           =  2,
  MD_THREAD_LIST_STREAM          =  3,  /* MDRawThreadList */
  MD_MODULE_LIST_STREAM          =  4,  /* MDRawModuleList */
  MD_MEMORY_LIST_STREAM          =  5,  /* MDRawMemoryList */
  MD_EXCEPTION_STREAM            =  6,  /* MDRawExceptionStream */
  MD_SYSTEM_INFO_STREAM          =  7,  /* MDRawSystemInfo */
  MD_THREAD_EX_LIST_STREAM       =  8,
  MD_MEMORY_64_LIST_STREAM       =  9,
  MD_COMMENT_STREAM_A            = 10,
  MD_COMMENT_STREAM_W            = 11,
  MD_HANDLE_DATA_STREAM          = 12,
  MD_FUNCTION_TABLE_STREAM       = 13,
  MD_UNLOADED_MODULE_LIST_STREAM = 14,
  MD_MISC_INFO_STREAM            = 15,  /* MDRawMiscInfo */
  MD_LAST_RESERVED_STREAM        = 0x0000ffff,

  /* Breakpad extension types.  0x4767 = "Gg" */
  MD_BREAKPAD_INFO_STREAM          = 0x47670001,  /* MDRawBreakpadInfo */
  MD_ASSERTION_INFO_STREAM       = 0x47670002   /* MDRawAssertionInfo */
} MDStreamType;  /* MINIDUMP_STREAM_TYPE */


typedef struct {
  u_int32_t length;     /* Length of buffer in bytes (not characters),
                         * excluding 0-terminator */
  u_int16_t buffer[1];  /* UTF-16-encoded, 0-terminated */
} MDString;  /* MINIDUMP_STRING */

static const size_t MDString_minsize = offsetof(MDString, buffer[0]);


typedef struct {
  u_int32_t            thread_id;
  u_int32_t            suspend_count;
  u_int32_t            priority_class;
  u_int32_t            priority;
  u_int64_t            teb;             /* Thread environment block */
  MDMemoryDescriptor   stack;
  MDLocationDescriptor thread_context;  /* MDRawContext[CPU] */
} MDRawThread;  /* MINIDUMP_THREAD */


typedef struct {
  u_int32_t   number_of_threads;
  MDRawThread threads[1];
} MDRawThreadList;  /* MINIDUMP_THREAD_LIST */

static const size_t MDRawThreadList_minsize = offsetof(MDRawThreadList,
                                                       threads[0]);


typedef struct {
  u_int64_t            base_of_image;
  u_int32_t            size_of_image;
  u_int32_t            checksum;         /* 0 if unknown */
  u_int32_t            time_date_stamp;  /* time_t */
  MDRVA                module_name_rva;  /* MDString, pathname or filename */
  MDVSFixedFileInfo    version_info;

  /* The next field stores a CodeView record and is populated when a module's
   * debug information resides in a PDB file.  It identifies the PDB file. */
  MDLocationDescriptor cv_record;

  /* The next field is populated when a module's debug information resides
   * in a DBG file.  It identifies the DBG file.  This field is effectively
   * obsolete with modules built by recent toolchains. */
  MDLocationDescriptor misc_record;

  /* Alignment problem: reserved0 and reserved1 are defined by the platform
   * SDK as 64-bit quantities.  However, that results in a structure whose
   * alignment is unpredictable on different CPUs and ABIs.  If the ABI
   * specifies full alignment of 64-bit quantities in structures (as ppc
   * does), there will be padding between miscRecord and reserved0.  If
   * 64-bit quantities can be aligned on 32-bit boundaries (as on x86),
   * this padding will not exist.  (Note that the structure up to this point
   * contains 1 64-bit member followed by 21 32-bit members.)
   * As a workaround, reserved0 and reserved1 are instead defined here as
   * four 32-bit quantities.  This should be harmless, as there are
   * currently no known uses for these fields. */
  u_int32_t            reserved0[2];
  u_int32_t            reserved1[2];
} MDRawModule;  /* MINIDUMP_MODULE */

/* The inclusion of a 64-bit type in MINIDUMP_MODULE forces the struct to
 * be tail-padded out to a multiple of 64 bits under some ABIs (such as PPC).
 * This doesn't occur on systems that don't tail-pad in this manner.  Define
 * this macro to be the usable size of the MDRawModule struct, and use it in
 * place of sizeof(MDRawModule). */
#define MD_MODULE_SIZE 108


/* (MDRawModule).cv_record can reference MDCVInfoPDB20 or MDCVInfoPDB70.
 * Ref.: http://www.debuginfo.com/articles/debuginfomatch.html
 * MDCVInfoPDB70 is the expected structure type with recent toolchains. */

typedef struct {
  u_int32_t signature;
  u_int32_t offset;     /* Offset to debug data (expect 0 in minidump) */
} MDCVHeader;

typedef struct {
  MDCVHeader cv_header;
  u_int32_t  signature;         /* time_t debug information created */
  u_int32_t  age;               /* revision of PDB file */
  u_int8_t   pdb_file_name[1];  /* Pathname or filename of PDB file */
} MDCVInfoPDB20;

static const size_t MDCVInfoPDB20_minsize = offsetof(MDCVInfoPDB20,
                                                     pdb_file_name[0]);

#define MD_CVINFOPDB20_SIGNATURE 0x3031424e  /* cvHeader.signature = '01BN' */

typedef struct {
  u_int32_t cv_signature;
  MDGUID    signature;         /* GUID, identifies PDB file */
  u_int32_t age;               /* Identifies incremental changes to PDB file */
  u_int8_t  pdb_file_name[1];  /* Pathname or filename of PDB file,
                                * 0-terminated 8-bit character data (UTF-8?) */
} MDCVInfoPDB70;

static const size_t MDCVInfoPDB70_minsize = offsetof(MDCVInfoPDB70,
                                                     pdb_file_name[0]);

#define MD_CVINFOPDB70_SIGNATURE 0x53445352  /* cvSignature = 'SDSR' */

/* In addition to the two CodeView record formats above, used for linking
 * to external pdb files, it is possible for debugging data to be carried
 * directly in the CodeView record itself.  These signature values will
 * be found in the first 4 bytes of the CodeView record.  Additional values
 * not commonly experienced in the wild are given by "Microsoft Symbol and
 * Type Information", http://www.x86.org/ftp/manuals/tools/sym.pdf, section
 * 7.2.  An in-depth description of the CodeView 4.1 format is given by
 * "Undocumented Windows 2000 Secrets", Windows 2000 Debugging Support/
 * Microsoft Symbol File Internals/CodeView Subsections,
 * http://www.rawol.com/features/undocumented/sbs-w2k-1-windows-2000-debugging-support.pdf
 */
#define MD_CVINFOCV41_SIGNATURE 0x3930424e  /* '90BN', CodeView 4.10. */
#define MD_CVINFOCV50_SIGNATURE 0x3131424e  /* '11BN', CodeView 5.0,
                                             * MS C7-format (/Z7). */

#define MD_CVINFOUNKNOWN_SIGNATURE 0xffffffff  /* An unlikely value. */

/* (MDRawModule).miscRecord can reference MDImageDebugMisc.  The Windows
 * structure is actually defined in WinNT.h.  This structure is effectively
 * obsolete with modules built by recent toolchains. */

typedef struct {
  u_int32_t data_type;    /* IMAGE_DEBUG_TYPE_*, not defined here because
                           * this debug record type is mostly obsolete. */
  u_int32_t length;       /* Length of entire MDImageDebugMisc structure */
  u_int8_t  unicode;      /* True if data is multibyte */
  u_int8_t  reserved[3];
  u_int8_t  data[1];
} MDImageDebugMisc;  /* IMAGE_DEBUG_MISC */

static const size_t MDImageDebugMisc_minsize = offsetof(MDImageDebugMisc,
                                                        data[0]);


typedef struct {
  u_int32_t   number_of_modules;
  MDRawModule modules[1];
} MDRawModuleList;  /* MINIDUMP_MODULE_LIST */

static const size_t MDRawModuleList_minsize = offsetof(MDRawModuleList,
                                                       modules[0]);


typedef struct {
  u_int32_t          number_of_memory_ranges;
  MDMemoryDescriptor memory_ranges[1];
} MDRawMemoryList;  /* MINIDUMP_MEMORY_LIST */

static const size_t MDRawMemoryList_minsize = offsetof(MDRawMemoryList,
                                                       memory_ranges[0]);


#define MD_EXCEPTION_MAXIMUM_PARAMETERS 15

typedef struct {
  u_int32_t exception_code;     /* Windows: MDExceptionCodeWin,
                                 * Mac OS X: MDExceptionMac,
                                 * Linux: MDExceptionCodeLinux. */
  u_int32_t exception_flags;    /* Windows: 1 if noncontinuable,
                                   Mac OS X: MDExceptionCodeMac. */
  u_int64_t exception_record;   /* Address (in the minidump-producing host's
                                 * memory) of another MDException, for
                                 * nested exceptions. */
  u_int64_t exception_address;  /* The address that caused the exception.
                                 * Mac OS X: exception subcode (which is
                                 *           typically the address). */
  u_int32_t number_parameters;  /* Number of valid elements in
                                 * exception_information. */
  u_int32_t __align;
  u_int64_t exception_information[MD_EXCEPTION_MAXIMUM_PARAMETERS];
} MDException;  /* MINIDUMP_EXCEPTION */

/* For (MDException).exception_code.  These values come from WinBase.h
 * and WinNT.h (names beginning with EXCEPTION_ are in WinBase.h,
 * they are STATUS_ in WinNT.h). */
typedef enum {
  MD_EXCEPTION_CODE_WIN_CONTROL_C                = 0x40010005,
      /* DBG_CONTROL_C */
  MD_EXCEPTION_CODE_WIN_GUARD_PAGE_VIOLATION     = 0x80000001,
      /* EXCEPTION_GUARD_PAGE */
  MD_EXCEPTION_CODE_WIN_DATATYPE_MISALIGNMENT    = 0x80000002,
      /* EXCEPTION_DATATYPE_MISALIGNMENT */
  MD_EXCEPTION_CODE_WIN_BREAKPOINT               = 0x80000003,
      /* EXCEPTION_BREAKPOINT */
  MD_EXCEPTION_CODE_WIN_SINGLE_STEP              = 0x80000004,
      /* EXCEPTION_SINGLE_STEP */
  MD_EXCEPTION_CODE_WIN_ACCESS_VIOLATION         = 0xc0000005,
      /* EXCEPTION_ACCESS_VIOLATION */
  MD_EXCEPTION_CODE_WIN_IN_PAGE_ERROR            = 0xc0000006,
      /* EXCEPTION_IN_PAGE_ERROR */
  MD_EXCEPTION_CODE_WIN_INVALID_HANDLE           = 0xc0000008,
      /* EXCEPTION_INVALID_HANDLE */
  MD_EXCEPTION_CODE_WIN_ILLEGAL_INSTRUCTION      = 0xc000001d,
      /* EXCEPTION_ILLEGAL_INSTRUCTION */
  MD_EXCEPTION_CODE_WIN_NONCONTINUABLE_EXCEPTION = 0xc0000025,
      /* EXCEPTION_NONCONTINUABLE_EXCEPTION */
  MD_EXCEPTION_CODE_WIN_INVALID_DISPOSITION      = 0xc0000026,
      /* EXCEPTION_INVALID_DISPOSITION */
  MD_EXCEPTION_CODE_WIN_ARRAY_BOUNDS_EXCEEDED    = 0xc000008c,
      /* EXCEPTION_BOUNDS_EXCEEDED */
  MD_EXCEPTION_CODE_WIN_FLOAT_DENORMAL_OPERAND   = 0xc000008d,
      /* EXCEPTION_FLT_DENORMAL_OPERAND */
  MD_EXCEPTION_CODE_WIN_FLOAT_DIVIDE_BY_ZERO     = 0xc000008e,
      /* EXCEPTION_FLT_DIVIDE_BY_ZERO */
  MD_EXCEPTION_CODE_WIN_FLOAT_INEXACT_RESULT     = 0xc000008f,
      /* EXCEPTION_FLT_INEXACT_RESULT */
  MD_EXCEPTION_CODE_WIN_FLOAT_INVALID_OPERATION  = 0xc0000090,
      /* EXCEPTION_FLT_INVALID_OPERATION */
  MD_EXCEPTION_CODE_WIN_FLOAT_OVERFLOW           = 0xc0000091,
      /* EXCEPTION_FLT_OVERFLOW */
  MD_EXCEPTION_CODE_WIN_FLOAT_STACK_CHECK        = 0xc0000092,
      /* EXCEPTION_FLT_STACK_CHECK */
  MD_EXCEPTION_CODE_WIN_FLOAT_UNDERFLOW          = 0xc0000093,
      /* EXCEPTION_FLT_UNDERFLOW */
  MD_EXCEPTION_CODE_WIN_INTEGER_DIVIDE_BY_ZERO   = 0xc0000094,
      /* EXCEPTION_INT_DIVIDE_BY_ZERO */
  MD_EXCEPTION_CODE_WIN_INTEGER_OVERFLOW         = 0xc0000095,
      /* EXCEPTION_INT_OVERFLOW */
  MD_EXCEPTION_CODE_WIN_PRIVILEGED_INSTRUCTION   = 0xc0000096,
      /* EXCEPTION_PRIV_INSTRUCTION */
  MD_EXCEPTION_CODE_WIN_STACK_OVERFLOW           = 0xc00000fd,
      /* EXCEPTION_STACK_OVERFLOW */
  MD_EXCEPTION_CODE_WIN_POSSIBLE_DEADLOCK        = 0xc0000194
      /* EXCEPTION_POSSIBLE_DEADLOCK */
} MDExceptionCodeWin;

/* For (MDException).exception_code.  Breakpad minidump extension for Mac OS X
 * support.  Based on Darwin/Mac OS X' mach/exception_types.h.  This is
 * what Mac OS X calls an "exception", not a "code". */
typedef enum {
  /* Exception code.  The high 16 bits of exception_code contains one of
   * these values. */
  MD_EXCEPTION_MAC_BAD_ACCESS      = 1,  /* code can be a kern_return_t */
      /* EXC_BAD_ACCESS */
  MD_EXCEPTION_MAC_BAD_INSTRUCTION = 2,  /* code is CPU-specific */
      /* EXC_BAD_INSTRUCTION */
  MD_EXCEPTION_MAC_ARITHMETIC      = 3,  /* code is CPU-specific */
      /* EXC_ARITHMETIC */
  MD_EXCEPTION_MAC_EMULATION       = 4,  /* code is CPU-specific */
      /* EXC_EMULATION */
  MD_EXCEPTION_MAC_SOFTWARE        = 5,
      /* EXC_SOFTWARE */
  MD_EXCEPTION_MAC_BREAKPOINT      = 6,  /* code is CPU-specific */
      /* EXC_BREAKPOINT */
  MD_EXCEPTION_MAC_SYSCALL         = 7,
      /* EXC_SYSCALL */
  MD_EXCEPTION_MAC_MACH_SYSCALL    = 8,
      /* EXC_MACH_SYSCALL */
  MD_EXCEPTION_MAC_RPC_ALERT       = 9
      /* EXC_RPC_ALERT */
} MDExceptionMac;

/* For (MDException).exception_flags.  Breakpad minidump extension for Mac OS X
 * support.  Based on Darwin/Mac OS X' mach/ppc/exception.h and
 * mach/i386/exception.h.  This is what Mac OS X calls a "code". */
typedef enum {
  /* With MD_EXCEPTION_BAD_ACCESS.  These are relevant kern_return_t values
   * from mach/kern_return.h. */
  MD_EXCEPTION_CODE_MAC_INVALID_ADDRESS    =  1,
      /* KERN_INVALID_ADDRESS */
  MD_EXCEPTION_CODE_MAC_PROTECTION_FAILURE =  2,
      /* KERN_PROTECTION_FAILURE */
  MD_EXCEPTION_CODE_MAC_NO_ACCESS          =  8,
      /* KERN_NO_ACCESS */
  MD_EXCEPTION_CODE_MAC_MEMORY_FAILURE     =  9,
      /* KERN_MEMORY_FAILURE */
  MD_EXCEPTION_CODE_MAC_MEMORY_ERROR       = 10,
      /* KERN_MEMORY_ERROR */

  /* With MD_EXCEPTION_SOFTWARE */
  MD_EXCEPTION_CODE_MAC_BAD_SYSCALL = 0x00010000,  /* Mach SIGSYS */
  MD_EXCEPTION_CODE_MAC_BAD_PIPE    = 0x00010001,  /* Mach SIGPIPE */
  MD_EXCEPTION_CODE_MAC_ABORT       = 0x00010002,  /* Mach SIGABRT */

  /* With MD_EXCEPTION_MAC_BAD_ACCESS on ppc */
  MD_EXCEPTION_CODE_MAC_PPC_VM_PROT_READ = 0x0101,
      /* EXC_PPC_VM_PROT_READ */
  MD_EXCEPTION_CODE_MAC_PPC_BADSPACE     = 0x0102,
      /* EXC_PPC_BADSPACE */
  MD_EXCEPTION_CODE_MAC_PPC_UNALIGNED    = 0x0103,
      /* EXC_PPC_UNALIGNED */

  /* With MD_EXCEPTION_MAC_BAD_INSTRUCTION on ppc */
  MD_EXCEPTION_CODE_MAC_PPC_INVALID_SYSCALL           = 1,
      /* EXC_PPC_INVALID_SYSCALL */
  MD_EXCEPTION_CODE_MAC_PPC_UNIMPLEMENTED_INSTRUCTION = 2,
      /* EXC_PPC_UNIPL_INST */
  MD_EXCEPTION_CODE_MAC_PPC_PRIVILEGED_INSTRUCTION    = 3,
      /* EXC_PPC_PRIVINST */
  MD_EXCEPTION_CODE_MAC_PPC_PRIVILEGED_REGISTER       = 4,
      /* EXC_PPC_PRIVREG */
  MD_EXCEPTION_CODE_MAC_PPC_TRACE                     = 5,
      /* EXC_PPC_TRACE */
  MD_EXCEPTION_CODE_MAC_PPC_PERFORMANCE_MONITOR       = 6,
      /* EXC_PPC_PERFMON */

  /* With MD_EXCEPTION_MAC_ARITHMETIC on ppc */
  MD_EXCEPTION_CODE_MAC_PPC_OVERFLOW           = 1,
      /* EXC_PPC_OVERFLOW */
  MD_EXCEPTION_CODE_MAC_PPC_ZERO_DIVIDE        = 2,
      /* EXC_PPC_ZERO_DIVIDE */
  MD_EXCEPTION_CODE_MAC_PPC_FLOAT_INEXACT      = 3,
      /* EXC_FLT_INEXACT */
  MD_EXCEPTION_CODE_MAC_PPC_FLOAT_ZERO_DIVIDE  = 4,
      /* EXC_PPC_FLT_ZERO_DIVIDE */
  MD_EXCEPTION_CODE_MAC_PPC_FLOAT_UNDERFLOW    = 5,
      /* EXC_PPC_FLT_UNDERFLOW */
  MD_EXCEPTION_CODE_MAC_PPC_FLOAT_OVERFLOW     = 6,
      /* EXC_PPC_FLT_OVERFLOW */
  MD_EXCEPTION_CODE_MAC_PPC_FLOAT_NOT_A_NUMBER = 7,
      /* EXC_PPC_FLT_NOT_A_NUMBER */

  /* With MD_EXCEPTION_MAC_EMULATION on ppc */
  MD_EXCEPTION_CODE_MAC_PPC_NO_EMULATION   = 8,
      /* EXC_PPC_NOEMULATION */
  MD_EXCEPTION_CODE_MAC_PPC_ALTIVEC_ASSIST = 9,
      /* EXC_PPC_ALTIVECASSIST */

  /* With MD_EXCEPTION_MAC_SOFTWARE on ppc */
  MD_EXCEPTION_CODE_MAC_PPC_TRAP    = 0x00000001,  /* EXC_PPC_TRAP */
  MD_EXCEPTION_CODE_MAC_PPC_MIGRATE = 0x00010100,  /* EXC_PPC_MIGRATE */

  /* With MD_EXCEPTION_MAC_BREAKPOINT on ppc */
  MD_EXCEPTION_CODE_MAC_PPC_BREAKPOINT = 1,  /* EXC_PPC_BREAKPOINT */

  /* With MD_EXCEPTION_MAC_BAD_INSTRUCTION on x86, see also x86 interrupt
   * values below. */
  MD_EXCEPTION_CODE_MAC_X86_INVALID_OPERATION = 1,  /* EXC_I386_INVOP */

  /* With MD_EXCEPTION_MAC_ARITHMETIC on x86 */
  MD_EXCEPTION_CODE_MAC_X86_DIV       = 1,  /* EXC_I386_DIV */
  MD_EXCEPTION_CODE_MAC_X86_INTO      = 2,  /* EXC_I386_INTO */
  MD_EXCEPTION_CODE_MAC_X86_NOEXT     = 3,  /* EXC_I386_NOEXT */
  MD_EXCEPTION_CODE_MAC_X86_EXTOVR    = 4,  /* EXC_I386_EXTOVR */
  MD_EXCEPTION_CODE_MAC_X86_EXTERR    = 5,  /* EXC_I386_EXTERR */
  MD_EXCEPTION_CODE_MAC_X86_EMERR     = 6,  /* EXC_I386_EMERR */
  MD_EXCEPTION_CODE_MAC_X86_BOUND     = 7,  /* EXC_I386_BOUND */
  MD_EXCEPTION_CODE_MAC_X86_SSEEXTERR = 8,  /* EXC_I386_SSEEXTERR */

  /* With MD_EXCEPTION_MAC_BREAKPOINT on x86 */
  MD_EXCEPTION_CODE_MAC_X86_SGL = 1,  /* EXC_I386_SGL */
  MD_EXCEPTION_CODE_MAC_X86_BPT = 2,  /* EXC_I386_BPT */

  /* With MD_EXCEPTION_MAC_BAD_INSTRUCTION on x86.  These are the raw
   * x86 interrupt codes.  Most of these are mapped to other Mach
   * exceptions and codes, are handled, or should not occur in user space.
   * A few of these will do occur with MD_EXCEPTION_MAC_BAD_INSTRUCTION. */
  /* EXC_I386_DIVERR    =  0: mapped to EXC_ARITHMETIC/EXC_I386_DIV */
  /* EXC_I386_SGLSTP    =  1: mapped to EXC_BREAKPOINT/EXC_I386_SGL */
  /* EXC_I386_NMIFLT    =  2: should not occur in user space */
  /* EXC_I386_BPTFLT    =  3: mapped to EXC_BREAKPOINT/EXC_I386_BPT */
  /* EXC_I386_INTOFLT   =  4: mapped to EXC_ARITHMETIC/EXC_I386_INTO */
  /* EXC_I386_BOUNDFLT  =  5: mapped to EXC_ARITHMETIC/EXC_I386_BOUND */
  /* EXC_I386_INVOPFLT  =  6: mapped to EXC_BAD_INSTRUCTION/EXC_I386_INVOP */
  /* EXC_I386_NOEXTFLT  =  7: should be handled by the kernel */
  /* EXC_I386_DBLFLT    =  8: should be handled (if possible) by the kernel */
  /* EXC_I386_EXTOVRFLT =  9: mapped to EXC_BAD_ACCESS/(PROT_READ|PROT_EXEC) */
  MD_EXCEPTION_CODE_MAC_X86_INVALID_TASK_STATE_SEGMENT = 10,
      /* EXC_INVTSSFLT */
  MD_EXCEPTION_CODE_MAC_X86_SEGMENT_NOT_PRESENT        = 11,
      /* EXC_SEGNPFLT */
  MD_EXCEPTION_CODE_MAC_X86_STACK_FAULT                = 12,
      /* EXC_STKFLT */
  MD_EXCEPTION_CODE_MAC_X86_GENERAL_PROTECTION_FAULT   = 13,
      /* EXC_GPFLT */
  /* EXC_I386_PGFLT     = 14: should not occur in user space */
  /* EXC_I386_EXTERRFLT = 16: mapped to EXC_ARITHMETIC/EXC_I386_EXTERR */
  MD_EXCEPTION_CODE_MAC_X86_ALIGNMENT_FAULT            = 17
      /* EXC_ALIGNFLT (for vector operations) */
  /* EXC_I386_ENOEXTFLT = 32: should be handled by the kernel */
  /* EXC_I386_ENDPERR   = 33: should not occur */
} MDExceptionCodeMac;

/* For (MDException).exception_code.  These values come from bits/signum.h.
 */
typedef enum {
  MD_EXCEPTION_CODE_LIN_SIGHUP = 1,      /* Hangup (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGINT = 2,      /* Interrupt (ANSI) */
  MD_EXCEPTION_CODE_LIN_SIGQUIT = 3,     /* Quit (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGILL = 4,      /* Illegal instruction (ANSI) */
  MD_EXCEPTION_CODE_LIN_SIGTRAP = 5,     /* Trace trap (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGABRT = 6,     /* Abort (ANSI) */
  MD_EXCEPTION_CODE_LIN_SIGBUS = 7,      /* BUS error (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGFPE = 8,      /* Floating-point exception (ANSI) */
  MD_EXCEPTION_CODE_LIN_SIGKILL = 9,     /* Kill, unblockable (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGUSR1 = 10,    /* User-defined signal 1 (POSIX).  */
  MD_EXCEPTION_CODE_LIN_SIGSEGV = 11,    /* Segmentation violation (ANSI) */
  MD_EXCEPTION_CODE_LIN_SIGUSR2 = 12,    /* User-defined signal 2 (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGPIPE = 13,    /* Broken pipe (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGALRM = 14,    /* Alarm clock (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGTERM = 15,    /* Termination (ANSI) */
  MD_EXCEPTION_CODE_LIN_SIGSTKFLT = 16,  /* Stack faultd */
  MD_EXCEPTION_CODE_LIN_SIGCHLD = 17,    /* Child status has changed (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGCONT = 18,    /* Continue (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGSTOP = 19,    /* Stop, unblockable (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGTSTP = 20,    /* Keyboard stop (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGTTIN = 21,    /* Background read from tty (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGTTOU = 22,    /* Background write to tty (POSIX) */
  MD_EXCEPTION_CODE_LIN_SIGURG = 23,
    /* Urgent condition on socket (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGXCPU = 24,    /* CPU limit exceeded (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGXFSZ = 25,
    /* File size limit exceeded (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGVTALRM = 26,  /* Virtual alarm clock (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGPROF = 27,    /* Profiling alarm clock (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGWINCH = 28,   /* Window size change (4.3 BSD, Sun) */
  MD_EXCEPTION_CODE_LIN_SIGIO = 29,      /* I/O now possible (4.2 BSD) */
  MD_EXCEPTION_CODE_LIN_SIGPWR = 30,     /* Power failure restart (System V) */
  MD_EXCEPTION_CODE_LIN_SIGSYS = 31      /* Bad system call */
} MDExceptionCodeLinux;

typedef struct {
  u_int32_t            thread_id;         /* Thread in which the exception
                                           * occurred.  Corresponds to
                                           * (MDRawThread).thread_id. */
  u_int32_t            __align;
  MDException          exception_record;
  MDLocationDescriptor thread_context;    /* MDRawContext[CPU] */
} MDRawExceptionStream;  /* MINIDUMP_EXCEPTION_STREAM */


typedef union {
  struct {
    u_int32_t vendor_id[3];               /* cpuid 0: ebx, edx, ecx */
    u_int32_t version_information;        /* cpuid 1: eax */
    u_int32_t feature_information;        /* cpuid 1: edx */
    u_int32_t amd_extended_cpu_features;  /* cpuid 0x80000001, ebx */
  } x86_cpu_info;
  struct {
    u_int64_t processor_features[2];
  } other_cpu_info;
} MDCPUInformation;  /* CPU_INFORMATION */


typedef struct {
  /* The next 3 fields and numberOfProcessors are from the SYSTEM_INFO
   * structure as returned by GetSystemInfo */
  u_int16_t        processor_architecture;
  u_int16_t        processor_level;         /* x86: 5 = 586, 6 = 686, ... */
  u_int16_t        processor_revision;      /* x86: 0xMMSS, where MM=model,
                                             *      SS=stepping */

  u_int8_t         number_of_processors;
  u_int8_t         product_type;            /* Windows: VER_NT_* from WinNT.h */

  /* The next 5 fields are from the OSVERSIONINFO structure as returned
   * by GetVersionEx */
  u_int32_t        major_version;
  u_int32_t        minor_version;
  u_int32_t        build_number;
  u_int32_t        platform_id;
  MDRVA            csd_version_rva;  /* MDString further identifying the
                                      * host OS.
                                      * Windows: name of the installed OS
                                      *          service pack.
                                      * Mac OS X: the Apple OS build number
                                      *           (sw_vers -buildVersion).
                                      * Linux: uname -srvmo */

  u_int16_t        suite_mask;       /* Windows: VER_SUITE_* from WinNT.h */
  u_int16_t        reserved2;

  MDCPUInformation cpu;
} MDRawSystemInfo;  /* MINIDUMP_SYSTEM_INFO */

/* For (MDRawSystemInfo).processor_architecture: */
typedef enum {
  MD_CPU_ARCHITECTURE_X86       =  0,  /* PROCESSOR_ARCHITECTURE_INTEL */
  MD_CPU_ARCHITECTURE_MIPS      =  1,  /* PROCESSOR_ARCHITECTURE_MIPS */
  MD_CPU_ARCHITECTURE_ALPHA     =  2,  /* PROCESSOR_ARCHITECTURE_ALPHA */
  MD_CPU_ARCHITECTURE_PPC       =  3,  /* PROCESSOR_ARCHITECTURE_PPC */
  MD_CPU_ARCHITECTURE_SHX       =  4,  /* PROCESSOR_ARCHITECTURE_SHX
                                        * (Super-H) */
  MD_CPU_ARCHITECTURE_ARM       =  5,  /* PROCESSOR_ARCHITECTURE_ARM */
  MD_CPU_ARCHITECTURE_IA64      =  6,  /* PROCESSOR_ARCHITECTURE_IA64 */
  MD_CPU_ARCHITECTURE_ALPHA64   =  7,  /* PROCESSOR_ARCHITECTURE_ALPHA64 */
  MD_CPU_ARCHITECTURE_MSIL      =  8,  /* PROCESSOR_ARCHITECTURE_MSIL
                                        * (Microsoft Intermediate Language) */
  MD_CPU_ARCHITECTURE_AMD64     =  9,  /* PROCESSOR_ARCHITECTURE_AMD64 */
  MD_CPU_ARCHITECTURE_X86_WIN64 = 10,
      /* PROCESSOR_ARCHITECTURE_IA32_ON_WIN64 (WoW64) */
  MD_CPU_ARCHITECTURE_UNKNOWN   = 0xffff  /* PROCESSOR_ARCHITECTURE_UNKNOWN */
} MDCPUArchitecture;

/* For (MDRawSystemInfo).platform_id: */
typedef enum {
  MD_OS_WIN32S        = 0,  /* VER_PLATFORM_WIN32s (Windows 3.1) */
  MD_OS_WIN32_WINDOWS = 1,  /* VER_PLATFORM_WIN32_WINDOWS (Windows 95-98-Me) */
  MD_OS_WIN32_NT      = 2,  /* VER_PLATFORM_WIN32_NT (Windows NT, 2000+) */
  MD_OS_WIN32_CE      = 3,  /* VER_PLATFORM_WIN32_CE, VER_PLATFORM_WIN32_HH
                             * (Windows CE, Windows Mobile, "Handheld") */

  /* The following values are Breakpad-defined. */
  MD_OS_UNIX          = 0x8000,  /* Generic Unix-ish */
  MD_OS_MAC_OS_X      = 0x8101,  /* Mac OS X/Darwin */
  MD_OS_LINUX         = 0x8201   /* Linux */
} MDOSPlatform;


typedef struct {
  u_int32_t size_of_info;  /* Length of entire MDRawMiscInfo structure. */
  u_int32_t flags1;

  /* The next field is only valid if flags1 contains
   * MD_MISCINFO_FLAGS1_PROCESS_ID. */
  u_int32_t process_id;

  /* The next 3 fields are only valid if flags1 contains
   * MD_MISCINFO_FLAGS1_PROCESS_TIMES. */
  u_int32_t process_create_time;  /* time_t process started */
  u_int32_t process_user_time;    /* seconds of user CPU time */
  u_int32_t process_kernel_time;  /* seconds of kernel CPU time */

  /* The following fields are not present in MINIDUMP_MISC_INFO but are
   * in MINIDUMP_MISC_INFO_2.  When this struct is populated, these values
   * may not be set.  Use flags1 or sizeOfInfo to determine whether these
   * values are present.  These are only valid when flags1 contains
   * MD_MISCINFO_FLAGS1_PROCESSOR_POWER_INFO. */
  u_int32_t processor_max_mhz;
  u_int32_t processor_current_mhz;
  u_int32_t processor_mhz_limit;
  u_int32_t processor_max_idle_state;
  u_int32_t processor_current_idle_state;
} MDRawMiscInfo;  /* MINIDUMP_MISC_INFO, MINIDUMP_MISC_INFO2 */

#define MD_MISCINFO_SIZE 24
#define MD_MISCINFO2_SIZE 44

/* For (MDRawMiscInfo).flags1.  These values indicate which fields in the
 * MDRawMiscInfoStructure are valid. */
typedef enum {
  MD_MISCINFO_FLAGS1_PROCESS_ID           = 0x00000001,
      /* MINIDUMP_MISC1_PROCESS_ID */
  MD_MISCINFO_FLAGS1_PROCESS_TIMES        = 0x00000002,
      /* MINIDUMP_MISC1_PROCESS_TIMES */
  MD_MISCINFO_FLAGS1_PROCESSOR_POWER_INFO = 0x00000004
      /* MINIDUMP_MISC1_PROCESSOR_POWER_INFO */
} MDMiscInfoFlags1;


/*
 * Breakpad extension types
 */


typedef struct {
  /* validity is a bitmask with values from MDBreakpadInfoValidity, indicating
   * which of the other fields in the structure are valid. */
  u_int32_t validity;

  /* Thread ID of the handler thread.  dump_thread_id should correspond to
   * the thread_id of an MDRawThread in the minidump's MDRawThreadList if
   * a dedicated thread in that list was used to produce the minidump.  If
   * the MDRawThreadList does not contain a dedicated thread used to produce
   * the minidump, this field should be set to 0 and the validity field
   * must not contain MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID. */
  u_int32_t dump_thread_id;

  /* Thread ID of the thread that requested the minidump be produced.  As
   * with dump_thread_id, requesting_thread_id should correspond to the
   * thread_id of an MDRawThread in the minidump's MDRawThreadList.  For
   * minidumps produced as a result of an exception, requesting_thread_id
   * will be the same as the MDRawExceptionStream's thread_id field.  For
   * minidumps produced "manually" at the program's request,
   * requesting_thread_id will indicate which thread caused the dump to be
   * written.  If the minidump was produced at the request of something
   * other than a thread in the MDRawThreadList, this field should be set
   * to 0 and the validity field must not contain
   * MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID. */
  u_int32_t requesting_thread_id;
} MDRawBreakpadInfo;

/* For (MDRawBreakpadInfo).validity: */
typedef enum {
  /* When set, the dump_thread_id field is valid. */
  MD_BREAKPAD_INFO_VALID_DUMP_THREAD_ID       = 1 << 0,

  /* When set, the requesting_thread_id field is valid. */
  MD_BREAKPAD_INFO_VALID_REQUESTING_THREAD_ID = 1 << 1
} MDBreakpadInfoValidity;

typedef struct {
  /* expression, function, and file are 0-terminated UTF-16 strings.  They
   * may be truncated if necessary, but should always be 0-terminated when
   * written to a file.
   * Fixed-length strings are used because MiniDumpWriteDump doesn't offer
   * a way for user streams to point to arbitrary RVAs for strings. */
  u_int16_t expression[128];  /* Assertion that failed... */
  u_int16_t function[128];    /* ...within this function... */
  u_int16_t file[128];        /* ...in this file... */
  u_int32_t line;             /* ...at this line. */
  u_int32_t type;
} MDRawAssertionInfo;

/* For (MDRawAssertionInfo).type: */
typedef enum {
  MD_ASSERTION_INFO_TYPE_UNKNOWN = 0,

  /* Used for assertions that would be raised by the MSVC CRT but are
   * directed to an invalid parameter handler instead. */
  MD_ASSERTION_INFO_TYPE_INVALID_PARAMETER,

  /* Used for assertions that would be raised by the MSVC CRT but are
   * directed to a pure virtual call handler instead. */
  MD_ASSERTION_INFO_TYPE_PURE_VIRTUAL_CALL
} MDAssertionInfoData;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif  /* _MSC_VER */


#endif  /* GOOGLE_BREAKPAD_COMMON_MINIDUMP_FORMAT_H__ */
