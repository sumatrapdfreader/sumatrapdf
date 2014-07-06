/* Copyright 2014 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#ifndef rar_rar_h
#define rar_rar_h

#include "../unarr-internals.h"

#include "../lzss.h"
#include "../ppmd/Ppmd7.h"
#include <limits.h>

typedef struct ar_archive_rar_s ar_archive_rar;

/***** parser *****/

enum block_types {
    TYPE_MAGIC = 0x72, TYPE_MAIN_HEADER = 0x73, TYPE_FILE_ENTRY = 0x74,
    TYPE_NEWSUB = 0x7A, TYPE_END_OF_ARCHIVE = 0x7B,
};

enum archive_flags {
    MHD_VOLUME = 1 << 0, MHD_COMMENT = 1 << 1, MHD_LOCK = 1 << 2,
    MHD_SOLID = 1 << 3, MHD_PACK_COMMENT = 1 << 4, MHD_AV = 1 << 5,
    MHD_PROTECT = 1 << 6, MHD_PASSWORD = 1 << 7, MHD_FIRSTVOLUME = 1 << 8,
    MHD_ENCRYPTVER = 1 << 9,
    MHD_LONG_BLOCK = 1 << 15,
};

enum entry_flags {
    LHD_SPLIT_BEFORE = 1 << 0, LHD_SPLIT_AFTER = 1 << 1, LHD_PASSWORD = 1 << 2,
    LHD_COMMENT = 1 << 3, LHD_SOLID = 1 << 4,
    LHD_DIRECTORY = (1 << 5) | (1 << 6) | (1 << 7),
    LHD_LARGE = 1 << 8, LHD_UNICODE = 1 << 9, LHD_SALT = 1 << 10,
    LHD_VERSION = 1 << 11, LHD_EXTTIME = 1 << 12, LHD_EXTFLAGS = 1 << 13,
    LHD_LONG_BLOCK = 1 << 15,
};

enum compression_method {
    METHOD_STORE = 0x30,
    METHOD_FASTEST = 0x31, METHOD_FAST = 0x32, METHOD_NORMAL = 0x33,
    METHOD_GOOD = 0x34, METHOD_BEST = 0x35,
};

struct rar_header {
    uint16_t crc;
    uint8_t type;
    uint16_t flags;
    uint16_t size;
    uint64_t datasize;
};

struct rar_entry {
    uint64_t size;
    uint8_t os;
    uint32_t crc;
    uint32_t dosdate;
    uint8_t version;
    uint8_t method;
    uint16_t namelen;
    uint32_t attrs;
    bool solid;
};

struct ar_archive_rar_entry {
    uint8_t method;
    uint32_t crc;
    uint16_t header_size;
    bool restart_solid;
    char *name;
    wchar16_t *name_w;
};

bool rar_parse_header(ar_archive *ar, struct rar_header *header);
bool rar_check_header_crc(ar_archive *ar);
bool rar_parse_header_entry(ar_archive_rar *rar, struct rar_header *header, struct rar_entry *entry);
const char *rar_get_name(ar_archive *ar);
const wchar16_t *rar_get_name_w(ar_archive *ar);

/***** filters *****/

struct RARVirtualMachine;
struct RARProgramCode;
struct RARFilter;

struct ar_archive_rar_filters {
    struct RARVirtualMachine *vm;
    struct RARProgramCode *progs;
    struct RARFilter *stack;
    size_t filterstart;
    uint32_t lastfilternum;
    size_t lastend;
    uint8_t *bytes;
    size_t bytes_ready;
};

bool rar_parse_filter(ar_archive_rar *rar, const uint8_t *bytes, uint16_t length, uint8_t flags);
bool rar_run_filters(ar_archive_rar *rar);
void rar_clear_filters(struct ar_archive_rar_filters *filters);

/***** uncompress *****/

#define MAINCODE_SIZE      299
#define OFFSETCODE_SIZE    60
#define LOWOFFSETCODE_SIZE 17
#define LENGTHCODE_SIZE    28
#define HUFFMAN_TABLE_SIZE MAINCODE_SIZE + OFFSETCODE_SIZE + LOWOFFSETCODE_SIZE + LENGTHCODE_SIZE

struct huffman_tree_node {
    int branches[2];
};

struct huffman_table_entry {
    int length;
    int value;
};

struct huffman_code {
    struct huffman_tree_node *tree;
    int numentries;
    int capacity;
    int minlength;
    int maxlength;
    int tablesize;
    struct huffman_table_entry *table;
};

struct ByteReader {
    IByteIn super;
    ar_archive_rar *rar;
};

struct CPpmdRAR_RangeDec {
    IPpmd7_RangeDec super;
    UInt32 Range;
    UInt32 Code;
    UInt32 Low;
    UInt32 Bottom;
    IByteIn *Stream;
};

struct ar_archive_rar_uncomp {
    bool initialized;
    bool start_new_table;

    LZSS lzss;
    uint32_t dict_size;
    size_t bytes_ready;
    struct huffman_code maincode;
    struct huffman_code offsetcode;
    struct huffman_code lowoffsetcode;
    struct huffman_code lengthcode;
    uint8_t lengthtable[HUFFMAN_TABLE_SIZE];
    uint32_t lastlength;
    uint32_t lastoffset;
    uint32_t oldoffset[4];
    uint32_t lastlowoffset;
    uint32_t numlowoffsetrepeats;

    bool is_ppmd_block;
    bool ppmd_valid;
    int ppmd_escape;
    CPpmd7 ppmd7_context;
    struct CPpmdRAR_RangeDec range_dec;
    struct ByteReader bytein;

    struct ar_archive_rar_filters filters;

    struct StreamBitReader {
        uint64_t bits;
        int available;
        bool at_eof;
    } br;
};

bool rar_uncompress_part(ar_archive_rar *rar, void *buffer, size_t buffer_size);
int64_t rar_expand(ar_archive_rar *rar, int64_t end);
void rar_clear_uncompress(struct ar_archive_rar_uncomp *uncomp);

/***** rar *****/

struct ar_archive_rar_progress {
    size_t data_left;
    size_t bytes_done;
    uint32_t crc;
};

struct ar_archive_rar_s {
    ar_archive super;
    uint16_t archive_flags;
    struct ar_archive_rar_entry entry;
    struct ar_archive_rar_uncomp uncomp;
    struct ar_archive_rar_progress progr;
};

#endif
