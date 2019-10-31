/*
 * bin2coff: converts a data object into a Win32 linkable COFF binary object
 * Copyright (c) 2011 Pete Batard <pete@akeo.ie>
 * This file is part of the libwdi project: http://libwdi.sf.net
 * Modifications Copyright (c) 2018 by Artifex Software
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * References:
 * http://www.vortex.masmcode.com/ (another bin2coff, without source)
 * http://msdn.microsoft.com/en-us/library/ms680198.aspx
 * http://webster.cs.ucr.edu/Page_TechDocs/pe.txt
 * http://www.delorie.com/djgpp/doc/coff/
 * http://pierrelib.pagesperso-orange.fr/exec_formats/MS_Symbol_Type_v1.0.pdf
 */

/*
  Updates from Artifex Software Inc.
    + Automatically rename '-' to '_' in generated symbols.
    + Accept 'Win32' and 'x64' as flags.
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if !defined(_MSC_VER)
#include <stdint.h>
#else
typedef signed char          int8_t;
typedef unsigned char        uint8_t;
typedef short                int16_t;
typedef unsigned short       uint16_t;
typedef int                  int32_t;
typedef unsigned             uint32_t;
typedef long long            int64_t;
typedef unsigned long long   uint64_t;
#endif

#define SIZE_LABEL_SUFFIX				 "_size"
#define SIZE_TYPE						 uint32_t

#define IMAGE_SIZEOF_SHORT_NAME			 8

/* File header defines */
#define IMAGE_FILE_MACHINE_ANY			 0x0000
#define IMAGE_FILE_MACHINE_I386			 0x014c
#define IMAGE_FILE_MACHINE_IA64			 0x0200
#define IMAGE_FILE_MACHINE_AMD64		 0x8664

#define IMAGE_FILE_RELOCS_STRIPPED		 0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE		 0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED	 0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED	 0x0008
#define IMAGE_FILE_AGGRESIVE_WS_TRIM	 0x0010		/* Obsolete */
#define IMAGE_FILE_LARGE_ADDRESS_AWARE	 0x0020
#define IMAGE_FILE_16BIT_MACHINE		 0x0040
#define IMAGE_FILE_BYTES_REVERSED_LO	 0x0080		/* Obsolete */
#define IMAGE_FILE_32BIT_MACHINE		 0x0100
#define IMAGE_FILE_DEBUG_STRIPPED		 0x0200
#define IMAGE_FILE_REM_RUN_FROM_SWAP	 0x0400
#define IMAGE_FILE_NET_RUN_FROM_SWAP	 0x0800
#define IMAGE_FILE_SYSTEM				 0x1000
#define IMAGE_FILE_DLL					 0x2000
#define IMAGE_FILE_UP_SYSTEM_ONLY		 0x4000
#define IMAGE_FILE_BYTES_REVERSED_HI	 0x8000		/* Obsolete */

/* Section header defines */
#define IMAGE_SCN_TYPE_REG				 0x00000000	/* Reserved */
#define IMAGE_SCN_TYPE_DSECT			 0x00000001	/* Reserved */
#define IMAGE_SCN_TYPE_NOLOAD			 0x00000002	/* Reserved */
#define IMAGE_SCN_TYPE_GROUP			 0x00000003	/* Reserved */
#define IMAGE_SCN_TYPE_NO_PAD			 0x00000008	/* Obsolete */
#define IMAGE_SCN_TYPE_COPY				 0x00000010	/* Reserved */
#define IMAGE_SCN_CNT_CODE				 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA	 0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA 0x00000080
#define IMAGE_SCN_LNK_OTHER				 0x00000100	/* Reserved */
#define IMAGE_SCN_LNK_INFO				 0x00000200
#define IMAGE_SCN_TYPE_OVER				 0x00000400	/* Reserved */
#define IMAGE_SCN_LNK_REMOVE			 0x00000800
#define IMAGE_SCN_LNK_COMDAT			 0x00001000
#define IMAGE_SCN_MEM_FARDATA			 0x00008000	/* Reserved */
#define IMAGE_SCN_MEM_PURGEABLE			 0x00020000	/* Reserved */
#define IMAGE_SCN_MEM_16BIT				 0x00020000	/* Reserved */
#define IMAGE_SCN_MEM_LOCKED			 0x00040000	/* Reserved */
#define IMAGE_SCN_MEM_PRELOAD			 0x00080000	/* Reserved */
#define IMAGE_SCN_ALIGN_1BYTES			 0x00100000
#define IMAGE_SCN_ALIGN_2BYTES			 0x00200000
#define IMAGE_SCN_ALIGN_4BYTES			 0x00300000
#define IMAGE_SCN_ALIGN_8BYTES			 0x00400000
#define IMAGE_SCN_ALIGN_16BYTES			 0x00500000
#define IMAGE_SCN_ALIGN_32BYTES			 0x00600000
#define IMAGE_SCN_ALIGN_64BYTES			 0x00700000
#define IMAGE_SCN_ALIGN_128BYTES		 0x00800000
#define IMAGE_SCN_ALIGN_256BYTES		 0x00900000
#define IMAGE_SCN_ALIGN_512BYTES		 0x00A00000
#define IMAGE_SCN_ALIGN_1024BYTES		 0x00B00000
#define IMAGE_SCN_ALIGN_2048BYTES		 0x00C00000
#define IMAGE_SCN_ALIGN_4096BYTES		 0x00D00000
#define IMAGE_SCN_ALIGN_8192BYTES		 0x00E00000
#define IMAGE_SCN_ALIGN_MASK			 0x00F00000
#define IMAGE_SCN_LNK_NRELOC_OVFL		 0x01000000
#define IMAGE_SCN_MEM_DISCARDABLE		 0x02000000
#define IMAGE_SCN_MEM_NOT_CACHED		 0x04000000
#define IMAGE_SCN_MEM_NOT_PAGED			 0x08000000
#define IMAGE_SCN_MEM_SHARED			 0x10000000
#define IMAGE_SCN_MEM_EXECUTE			 0x20000000
#define IMAGE_SCN_MEM_READ				 0x40000000
#define IMAGE_SCN_MEM_WRITE				 0x80000000

/* Symbol entry defines */
#define IMAGE_SYM_UNDEFINED				 (int16_t)0
#define IMAGE_SYM_ABSOLUTE				 (int16_t)-1
#define IMAGE_SYM_DEBUG					 (int16_t)-2

#define IMAGE_SYM_TYPE_NULL				 0x0000
#define IMAGE_SYM_TYPE_VOID				 0x0001
#define IMAGE_SYM_TYPE_CHAR				 0x0002
#define IMAGE_SYM_TYPE_SHORT			 0x0003
#define IMAGE_SYM_TYPE_INT				 0x0004
#define IMAGE_SYM_TYPE_LONG				 0x0005
#define IMAGE_SYM_TYPE_FLOAT			 0x0006
#define IMAGE_SYM_TYPE_DOUBLE			 0x0007
#define IMAGE_SYM_TYPE_STRUCT			 0x0008
#define IMAGE_SYM_TYPE_UNION			 0x0009
#define IMAGE_SYM_TYPE_ENUM				 0x000A
#define IMAGE_SYM_TYPE_MOE				 0x000B
#define IMAGE_SYM_TYPE_BYTE				 0x000C
#define IMAGE_SYM_TYPE_WORD				 0x000D
#define IMAGE_SYM_TYPE_UINT				 0x000E
#define IMAGE_SYM_TYPE_DWORD			 0x000F
#define IMAGE_SYM_TYPE_PCODE			 0x8000

#define IMAGE_SYM_DTYPE_NULL			 0
#define IMAGE_SYM_DTYPE_POINTER			 1
#define IMAGE_SYM_DTYPE_FUNCTION		 2
#define IMAGE_SYM_DTYPE_ARRAY			 3

#define IMAGE_SYM_CLASS_END_OF_FUNCTION	 (uint8_t)-1
#define IMAGE_SYM_CLASS_NULL			 0x00
#define IMAGE_SYM_CLASS_AUTOMATIC		 0x01
#define IMAGE_SYM_CLASS_EXTERNAL		 0x02
#define IMAGE_SYM_CLASS_STATIC			 0x03
#define IMAGE_SYM_CLASS_REGISTER		 0x04
#define IMAGE_SYM_CLASS_EXTERNAL_DEF	 0x05
#define IMAGE_SYM_CLASS_LABEL			 0x06
#define IMAGE_SYM_CLASS_UNDEFINED_LABEL	 0x07
#define IMAGE_SYM_CLASS_MEMBER_OF_STRUCT 0x08
#define IMAGE_SYM_CLASS_ARGUMENT		 0x09
#define IMAGE_SYM_CLASS_STRUCT_TAG		 0x0A
#define IMAGE_SYM_CLASS_MEMBER_OF_UNION	 0x0B
#define IMAGE_SYM_CLASS_UNION_TAG		 0x0C
#define IMAGE_SYM_CLASS_TYPE_DEFINITION	 0x0D
#define IMAGE_SYM_CLASS_UNDEFINED_STATIC 0x0E
#define IMAGE_SYM_CLASS_ENUM_TAG		 0x0F
#define IMAGE_SYM_CLASS_MEMBER_OF_ENUM	 0x10
#define IMAGE_SYM_CLASS_REGISTER_PARAM	 0x11
#define IMAGE_SYM_CLASS_BIT_FIELD		 0x12
#define IMAGE_SYM_CLASS_FAR_EXTERNAL	 0x44
#define IMAGE_SYM_CLASS_BLOCK			 0x64
#define IMAGE_SYM_CLASS_FUNCTION		 0x65
#define IMAGE_SYM_CLASS_END_OF_STRUCT	 0x66
#define IMAGE_SYM_CLASS_FILE			 0x67
#define IMAGE_SYM_CLASS_SECTION			 0x68
#define IMAGE_SYM_CLASS_WEAK_EXTERNAL	 0x69
#define IMAGE_SYM_CLASS_CLR_TOKEN		 0x6B

#pragma pack(push, 2)

/* Microsoft COFF File Header */
typedef struct {
	uint16_t	Machine;
	uint16_t	NumberOfSections;
	uint32_t	TimeDateStamp;
	uint32_t	PointerToSymbolTable;
	uint32_t	NumberOfSymbols;
	uint16_t	SizeOfOptionalHeader;
	uint16_t	Characteristics;
} IMAGE_FILE_HEADER;

/* Microsoft COFF Section Header */
typedef struct {
	char Name[IMAGE_SIZEOF_SHORT_NAME];
	uint32_t	VirtualSize;
	uint32_t	VirtualAddress;
	uint32_t	SizeOfRawData;
	uint32_t	PointerToRawData;
	uint32_t	PointerToRelocations;
	uint32_t	PointerToLinenumbers;
	uint16_t	NumberOfRelocations;
	uint16_t	NumberOfLinenumbers;
	uint32_t	Characteristics;
} IMAGE_SECTION_HEADER;

/* Microsoft COFF Symbol Entry */
typedef struct {
	union {
		char	ShortName[IMAGE_SIZEOF_SHORT_NAME];
		struct {
			uint32_t Zeroes;
			uint32_t Offset;
		} LongName;
	} N;
	int32_t		Value;
	int16_t		SectionNumber;
	uint16_t	Type;
	uint8_t		StorageClass;
	uint8_t		NumberOfAuxSymbols;
} IMAGE_SYMBOL;

/* COFF String Table */
typedef struct {
	uint32_t	TotalSize;
	char		Strings[0];
} IMAGE_STRINGS;

#pragma pack(pop)

static int check_64bit(const char *arg, int *x86_32)
{
	if ((strcmp(arg, "64bit") == 0) || (strcmp(arg, "x64") == 0))
		*x86_32 = 0; /* 0 = 64bit */
	else if ((strcmp(arg, "32bit") == 0) || (strcmp(arg, "Win32") == 0))
		*x86_32 = 1; /* 1 = 32bit */
	else
		return 0;
	return 1;
}

int
#ifdef DDKBUILD
__cdecl
#endif
main (int argc, char *argv[])
{
	const uint16_t endian_test = 0xBE00;
	int x86_32, short_label, short_size, last_arg;
	int i, r = 1;
	char* label;
	FILE *fd = NULL;
	size_t size, alloc_size;
	uint8_t* buffer = NULL;
	IMAGE_FILE_HEADER* file_header;
	IMAGE_SECTION_HEADER* section_header;
	IMAGE_SYMBOL* symbol_table;
	IMAGE_STRINGS* string_table;
	SIZE_TYPE* data_size;

	if ((argc < 3) || (argc > 5)) {
		fprintf(stderr, "\nUsage: bin2coff bin obj [label] [64bit|Win32|x64]\n\n");
		fprintf(stderr, "  bin  : source binary data\n");
		fprintf(stderr, "  obj  : target object file, in MS COFF format.\n");
		fprintf(stderr, "  label: identifier for the extern data. If not provided, the name of the\n");
		fprintf(stderr, "         binary file without extension is used.\n");
		fprintf(stderr, "  64bit:\n  Win32:\n  x64  : produce a 64 bit compatible object - symbols are generated without\n");
		fprintf(stderr, "         leading underscores and machine type is set to x86_x64.\n\n");
		fprintf(stderr, "With your linker set properly, typical access from a C source is:\n\n");
		fprintf(stderr, "    extern uint8_t  label[]     /* binary data         */\n");
		fprintf(stderr, "    extern uint32_t label_size  /* size of binary data */\n\n");
		exit(1);
	}

	if (((uint8_t*)&endian_test)[0] == 0xBE) {
		fprintf(stderr, "\nThis program is not compatible with Big Endian architectures.\n");
		fprintf(stderr, "You are welcome to modify the sourcecode (GPLv3+) to make it so.\n");
		exit(1);
	}

	fd = fopen(argv[1], "rb");
	if (fd == NULL) {
		fprintf(stderr, "Couldn't open file '%s'.\n", argv[1]);
		goto err;
	}
	fseek(fd, 0, SEEK_END);
	size = (size_t)ftell(fd);
	fseek(fd, 0, SEEK_SET);

	x86_32 = 0;
	last_arg = argc;
	if (argc >= 4 && check_64bit(argv[3], &x86_32))
		last_arg = 4;
	else if (argc >= 5 && check_64bit(argv[4], &x86_32))
		last_arg = 5;

	/* Label setup */
	if (argc < last_arg) {
		for (i=(int)strlen(argv[1])-1; i>=0; i--) {
			if (argv[1][i] == '.') {
				argv[1][i] = 0;
				break;
			}
		}
		label = argv[1];
	} else {
		label = argv[3];
	}

	{
		char *s = label;

		while (*s) {
			if (*s == '-')
				*s = '_';
			s++;
		}
	}

	short_label = (strlen(label) + x86_32) <= IMAGE_SIZEOF_SHORT_NAME;
	short_size = (strlen(label) + x86_32 + strlen(SIZE_LABEL_SUFFIX)) <= IMAGE_SIZEOF_SHORT_NAME;
	alloc_size = sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER) + size + sizeof(SIZE_TYPE) + 2*sizeof(IMAGE_SYMBOL) + sizeof(IMAGE_STRINGS);
	if (!short_label) {
		alloc_size += x86_32 + strlen(label) + 1;
	}
	if (!short_size) {
		alloc_size += x86_32 + strlen(label) + strlen(SIZE_LABEL_SUFFIX) + 1;
	}

	buffer = (uint8_t*)calloc(alloc_size, 1);
	if (buffer == NULL) {
		fprintf(stderr, "Couldn't allocate buffer.\n");
		goto err;
	}
	file_header = (IMAGE_FILE_HEADER*)&buffer[0];
	section_header = (IMAGE_SECTION_HEADER*)&buffer[sizeof(IMAGE_FILE_HEADER)];
	symbol_table = (IMAGE_SYMBOL*)&buffer[sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER) + size + sizeof(SIZE_TYPE)];
	string_table = (IMAGE_STRINGS*)&buffer[sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER) + size + sizeof(SIZE_TYPE) + 2*sizeof(IMAGE_SYMBOL)];

	/* Populate file header */
	file_header->Machine = (x86_32)?IMAGE_FILE_MACHINE_I386:IMAGE_FILE_MACHINE_AMD64;
	file_header->NumberOfSections = 1;
	file_header->PointerToSymbolTable = sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER) + (uint32_t)size+4;
	file_header->NumberOfSymbols = 2;
	file_header->Characteristics = IMAGE_FILE_LINE_NUMS_STRIPPED;

	/* Populate data section header */
	strncpy(section_header->Name, ".data", IMAGE_SIZEOF_SHORT_NAME);
	section_header->SizeOfRawData = (uint32_t)size+4;
	section_header->PointerToRawData = sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER);
	section_header->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_ALIGN_16BYTES | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

	/* Populate data section */
	if (fread(&buffer[sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER)], 1, size, fd) != size) {
		fprintf(stderr, "Couldn't read file '%s'.\n", argv[1]);
		goto err;
	}
	fclose(fd); fd = NULL;
	data_size = (SIZE_TYPE*)&buffer[sizeof(IMAGE_FILE_HEADER) + sizeof(IMAGE_SECTION_HEADER) + size];
	*data_size = (SIZE_TYPE)size;

	/* Populate symbol table */
	if (short_label) {
		symbol_table[0].N.ShortName[0] = '_';
		strcpy(&symbol_table[0].N.ShortName[x86_32], label);
	} else {
		symbol_table[0].N.LongName.Zeroes = 0;
		symbol_table[0].N.LongName.Offset = sizeof(IMAGE_STRINGS);
	}
	/* Ideally, we would use (IMAGE_SYM_DTYPE_ARRAY << 8) | IMAGE_SYM_TYPE_BYTE
	 * to indicate an array of bytes, but the type is ignored in MS objects. */
	symbol_table[0].Type = IMAGE_SYM_TYPE_NULL;
	symbol_table[0].StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
	symbol_table[0].SectionNumber = 1;
	symbol_table[0].Value = 0;				/* Offset within the section */

	if (short_size) {
		symbol_table[1].N.ShortName[1] = '_';
		strcpy(&symbol_table[1].N.ShortName[x86_32], label);
		strcpy(&symbol_table[1].N.ShortName[x86_32+strlen(label)], SIZE_LABEL_SUFFIX);
	} else {
		symbol_table[1].N.LongName.Zeroes = 0;
		symbol_table[1].N.LongName.Offset = sizeof(IMAGE_STRINGS) + ((short_label)?0:(x86_32 + (uint32_t)strlen(label) + 1));
	}
	symbol_table[1].Type = IMAGE_SYM_TYPE_NULL;
	symbol_table[1].StorageClass = IMAGE_SYM_CLASS_EXTERNAL;
	symbol_table[1].SectionNumber = 1;
	symbol_table[1].Value = (int32_t)size;	/* Offset within the section */

	/* Populate string table */
	string_table->TotalSize = sizeof(IMAGE_STRINGS);
	if (!short_label) {
		string_table->Strings[0] = '_';
		strcpy(&string_table->Strings[0] + x86_32, label);
		string_table->TotalSize += x86_32 + (uint32_t)strlen(label) + 1;
	}
	if (!short_size) {
		string_table->Strings[string_table->TotalSize - sizeof(IMAGE_STRINGS)] = '_';
		strcpy(&string_table->Strings[string_table->TotalSize - sizeof(IMAGE_STRINGS)] + x86_32, label);
		string_table->TotalSize += x86_32 + (uint32_t)strlen(label);
		strcpy(&string_table->Strings[string_table->TotalSize - sizeof(IMAGE_STRINGS)], SIZE_LABEL_SUFFIX);
		string_table->TotalSize += (uint32_t)strlen(SIZE_LABEL_SUFFIX) + 1;
	}

	fd = fopen(argv[2], "wb");
	if (fd == NULL) {
		fprintf(stderr, "Couldn't create file '%s'.\n", argv[2]);
		goto err;
	}

	if (fwrite(buffer, 1, alloc_size, fd) != alloc_size) {
		fprintf(stderr, "Couldn't write file '%s'.\n", argv[2]);
		goto err;
	}
	printf("Successfully created COFF object file '%s'\n", argv[2]);

	r = 0;

err:
	if (fd != NULL) fclose(fd);
	free(buffer);
	exit(r);
}
