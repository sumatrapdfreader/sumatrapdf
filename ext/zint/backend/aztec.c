/* aztec.c - Handles Aztec 2D Symbols */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2024 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "aztec.h"
#include "reedsol.h"

#define AZTEC_MAX_CAPACITY  19968 /* ISO/IEC 24778:2008 5.3 Table 1 Maximum Symbol Bit Capacity */
/* Allow up to absolute minimum 3 ECC codewords, but now warn if results in less than the 5% minimum (ISO/IEC
   24778:2008 4.1.e) - previously could go down to 3 ECC codewords anyway if version given, due to bit-stuffing */
#define AZTEC_BIN_CAPACITY  19932 /* AZTEC_MAX_CAPACITY less 3 * 12 = 36 */
#define AZTEC_MAP_SIZE      22801 /* AztecMap Version 32 151 x 151 */
#define AZTEC_MAP_POSN_MAX  20039 /* Maximum position index in AztecMap */

#define AZ_BIN_CAP_CWDS_S   "1661" /* String version of (AZTEC_BIN_CAPACITY / 12) */

/* Count number of consecutive (. SP) or (, SP) Punct mode doubles for comparison against Digit mode encoding */
static int az_count_doubles(const unsigned char source[], int i, const int length) {
    int c = 0;

    while ((i + 1 < length) && ((source[i] == '.') || (source[i] == ',')) && (source[i + 1] == ' ')) {
        c++;
        i += 2;
    }

    return c;
}

/* Count number of consecutive full stops or commas (can be encoded in Punct or Digit mode) */
static int az_count_dotcomma(const unsigned char source[], int i, const int length) {
    int c = 0;

    while (i < length && ((source[i] == '.') || (source[i] == ','))) {
        c++;
        i++;
    }

    return c;
}

/* Count number of consecutive `chr`s */
static int az_count_chr(const unsigned char source[], int i, const int length, const unsigned char chr) {
    int c = 0;

    while (i < length && source[i] == chr) {
        c++;
        i++;
    }

    return c;
}

/* Return mode following current, or 'E' if none */
static char az_get_next_mode(const char encode_mode[], const int src_len, int i) {
    int current_mode = encode_mode[i];

    do {
        i++;
    } while ((i < src_len) && (encode_mode[i] == current_mode));
    if (i >= src_len) {
        return 'E';
    } else {
        return encode_mode[i];
    }
}

/* Same as `bin_append_posn()`, except check for buffer overflow first */
static int az_bin_append_posn(const int arg, const int length, char *binary, const int bin_posn) {

    if (bin_posn + length > AZTEC_BIN_CAPACITY) {
        return 0; /* Fail */
    }
    return bin_append_posn(arg, length, binary, bin_posn);
}

/* Determine encoding modes and encode */
static int aztec_text_process(const unsigned char source[], int src_len, int bp, char binary_string[], const int gs1,
            const int eci, char *p_current_mode, int *data_length, const int debug_print) {

    int i, j;
    const char initial_mode = p_current_mode ? *p_current_mode : 'U';
    char current_mode;
    int count;
    char next_mode;
    int reduced_length;
    char *encode_mode = (char *) z_alloca(src_len + 1);
    unsigned char *reduced_source = (unsigned char *) z_alloca(src_len + 1);
    char *reduced_encode_mode = (char *) z_alloca(src_len + 1);

    for (i = 0; i < src_len; i++) {
        if (source[i] >= 128) {
            encode_mode[i] = 'B';
        } else if (gs1 && source[i] == '\x1D') {
            encode_mode[i] = 'P'; /* For FLG(n) & FLG(0) = FNC1 */
        } else {
            encode_mode[i] = AztecModes[source[i]];
        }
    }

    /* Deal first with letter combinations which can be combined to one codeword
       Combinations are (CR LF) (. SP) (, SP) (: SP) in Punct mode */
    current_mode = initial_mode;
    for (i = 0; i + 1 < src_len; i++) {
        /* Combination (CR LF) should always be in Punct mode */
        if ((source[i] == 13) && (source[i + 1] == 10)) {
            encode_mode[i] = 'P';
            encode_mode[i + 1] = 'P';

        /* Combination (: SP) should always be in Punct mode */
        } else if ((source[i] == ':') && (source[i + 1] == ' ')) {
            encode_mode[i + 1] = 'P';

        /* Combinations (. SP) and (, SP) sometimes use fewer bits in Digit mode */
        } else if (((source[i] == '.') || (source[i] == ',')) && (source[i + 1] == ' ') && (encode_mode[i] == 'X')) {
            count = az_count_doubles(source, i, src_len);
            next_mode = az_get_next_mode(encode_mode, src_len, i);

            if (current_mode == 'U') {
                if ((next_mode == 'D') && (count <= 5)) {
                    memset(encode_mode + i, 'D', 2 * count);
                }

            } else if (current_mode == 'L') {
                if ((next_mode == 'D') && (count <= 4)) {
                    memset(encode_mode + i, 'D', 2 * count);
                }

            } else if (current_mode == 'M') {
                if ((next_mode == 'D') && (count == 1)) {
                    encode_mode[i] = 'D';
                    encode_mode[i + 1] = 'D';
                }

            } else if (current_mode == 'D') {
                if ((next_mode != 'D') && (count <= 4)) {
                    memset(encode_mode + i, 'D', 2 * count);
                } else if ((next_mode == 'D') && (count <= 7)) {
                    memset(encode_mode + i, 'D', 2 * count);
                }
            }

            /* Default is Punct mode */
            if (encode_mode[i] == 'X') {
                encode_mode[i] = 'P';
                encode_mode[i + 1] = 'P';
            }
        }

        if ((encode_mode[i] != 'X') && (encode_mode[i] != 'B')) {
            current_mode = encode_mode[i];
        }
    }

    if (debug_print) {
        fputs("First Pass:\n", stdout);
        printf("%.*s\n", src_len, encode_mode);
    }

    /* Reduce two letter combinations to one codeword marked as [abcd] in Punct mode */
    i = 0;
    j = 0;
    while (i < src_len) {
        reduced_encode_mode[j] = encode_mode[i];
        if (i + 1 < src_len) {
            if ((source[i] == 13) && (source[i + 1] == 10)) { /* CR LF */
                reduced_source[j] = 'a';
                i += 2;
            } else if ((source[i] == '.') && (source[i + 1] == ' ') && (encode_mode[i] == 'P')) {
                reduced_source[j] = 'b';
                i += 2;
            } else if ((source[i] == ',') && (source[i + 1] == ' ') && (encode_mode[i] == 'P')) {
                reduced_source[j] = 'c';
                i += 2;
            } else if ((source[i] == ':') && (source[i + 1] == ' ')) {
                reduced_source[j] = 'd';
                i += 2;
            } else {
                reduced_source[j] = source[i++];
            }
        } else {
            reduced_source[j] = source[i++];
        }
        j++;
    }

    reduced_length = j;

    current_mode = initial_mode;
    for (i = 0; i < reduced_length; i++) {
        /* Resolve Carriage Return (CR) which can be Punct or Mixed mode */
        if (reduced_source[i] == 13) {
            count = az_count_chr(reduced_source, i, reduced_length, 13);
            next_mode = az_get_next_mode(reduced_encode_mode, reduced_length, i);

            if ((current_mode == 'U') && ((next_mode == 'U') || (next_mode == 'B')) && (count == 1)) {
                reduced_encode_mode[i] = 'P';

            } else if ((current_mode == 'L') && ((next_mode == 'L') || (next_mode == 'B')) && (count == 1)) {
                reduced_encode_mode[i] = 'P';

            } else if ((current_mode == 'P') || (next_mode == 'P')) {
                reduced_encode_mode[i] = 'P';
            }

            if (current_mode == 'D') {
                if (((next_mode == 'E') || (next_mode == 'U') || (next_mode == 'D') || (next_mode == 'B'))
                        && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'P', count);
                } else if ((next_mode == 'L') && (count == 1)) {
                    reduced_encode_mode[i] = 'P';
                }
            }

            /* Default is Mixed mode */
            if (reduced_encode_mode[i] == 'X') {
                reduced_encode_mode[i] = 'M';
            }

        /* Resolve full stop and comma which can be in Punct or Digit mode */
        } else if ((reduced_source[i] == '.') || (reduced_source[i] == ',')) {
            count = az_count_dotcomma(reduced_source, i, reduced_length);
            next_mode = az_get_next_mode(reduced_encode_mode, reduced_length, i);

            if (current_mode == 'U') {
                if (((next_mode == 'U') || (next_mode == 'L') || (next_mode == 'M') || (next_mode == 'B'))
                        && (count == 1)) {
                    reduced_encode_mode[i] = 'P';
                }

            } else if (current_mode == 'L') {
                if ((next_mode == 'L') && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'P', count);
                } else if (((next_mode == 'M') || (next_mode == 'B')) && (count == 1)) {
                    reduced_encode_mode[i] = 'P';
                }

            } else if (current_mode == 'M') {
                if (((next_mode == 'E') || (next_mode == 'U') || (next_mode == 'L') || (next_mode == 'M'))
                        && (count <= 4)) {
                    memset(reduced_encode_mode + i, 'P', count);
                } else if ((next_mode == 'B') && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'P', count);
                }

            } else if ((current_mode == 'P') && (next_mode != 'D') && (count <= 9)) {
                memset(reduced_encode_mode + i, 'P', count);
            }

            /* Default is Digit mode */
            if (reduced_encode_mode[i] == 'X') {
                reduced_encode_mode[i] = 'D';
            }

        /* Resolve Space (SP) which can be any mode except Punct */
        } else if (reduced_source[i] == ' ') {
            count = az_count_chr(reduced_source, i, reduced_length, ' ');
            next_mode = az_get_next_mode(reduced_encode_mode, reduced_length, i);

            if (current_mode == 'U') {
                if ((next_mode == 'E') && (count <= 5)) {
                    memset(reduced_encode_mode + i, 'U', count);
                } else if (((next_mode == 'U') || (next_mode == 'L') || (next_mode == 'M') || (next_mode == 'P')
                        || (next_mode == 'B')) && (count <= 9)) {
                    memset(reduced_encode_mode + i, 'U', count);
                }

            } else if (current_mode == 'L') {
                if ((next_mode == 'E') && (count <= 5)) {
                    memset(reduced_encode_mode + i, 'L', count);

                } else if ((next_mode == 'U') && (count == 1)) {
                    reduced_encode_mode[i] = 'L';

                } else if ((next_mode == 'L') && (count <= 14)) {
                    memset(reduced_encode_mode + i, 'L', count);

                } else if (((next_mode == 'M') || (next_mode == 'P') || (next_mode == 'B')) && (count <= 9)) {
                    memset(reduced_encode_mode + i, 'L', count);
                }

            } else if (current_mode == 'M') {
                if (((next_mode == 'E') || (next_mode == 'U')) && (count <= 9)) {
                    memset(reduced_encode_mode + i, 'M', count);

                } else if (((next_mode == 'L') || (next_mode == 'B')) && (count <= 14)) {
                    memset(reduced_encode_mode + i, 'M', count);

                } else if (((next_mode == 'M') || (next_mode == 'P')) && (count <= 19)) {
                    memset(reduced_encode_mode + i, 'M', count);
                }

            } else if (current_mode == 'P') {
                if ((next_mode == 'E') && (count <= 5)) {
                    memset(reduced_encode_mode + i, 'U', count);

                } else if (((next_mode == 'U') || (next_mode == 'L') || (next_mode == 'M') || (next_mode == 'P')
                        || (next_mode == 'B')) && (count <= 9)) {
                    memset(reduced_encode_mode + i, 'U', count);
                }
            }

            /* Default is Digit mode */
            if (reduced_encode_mode[i] == 'X') {
                reduced_encode_mode[i] = 'D';
            }
        }

        if (reduced_encode_mode[i] != 'B') {
            current_mode = reduced_encode_mode[i];
        }
    }

    /* Decide when to use P/S instead of P/L and U/S instead of U/L */
    current_mode = initial_mode;
    for (i = 0; i < reduced_length; i++) {

        if (reduced_encode_mode[i] != current_mode) {

            for (count = 0; ((i + count) < reduced_length)
                            && (reduced_encode_mode[i + count] == reduced_encode_mode[i]); count++);
            next_mode = az_get_next_mode(reduced_encode_mode, reduced_length, i);

            if (reduced_encode_mode[i] == 'P') {
                if ((current_mode == 'U') && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'p', count);

                } else if ((current_mode == 'L') && (next_mode != 'U') && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'p', count);

                } else if ((current_mode == 'L') && (next_mode == 'U') && (count == 1)) {
                    reduced_encode_mode[i] = 'p';

                } else if ((current_mode == 'M') && (next_mode != 'M') && (count == 1)) {
                    reduced_encode_mode[i] = 'p';

                } else if ((current_mode == 'M') && (next_mode == 'M') && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'p', count);

                } else if ((current_mode == 'D') && (next_mode != 'D') && (count <= 3)) {
                    memset(reduced_encode_mode + i, 'p', count);

                } else if ((current_mode == 'D') && (next_mode == 'D') && (count <= 6)) {
                    memset(reduced_encode_mode + i, 'p', count);
                }

            } else if (reduced_encode_mode[i] == 'U') {
                if ((current_mode == 'L') && ((next_mode == 'L') || (next_mode == 'M')) && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'u', count);

                } else if ((current_mode == 'L') && ((next_mode == 'E') || (next_mode == 'D') || (next_mode == 'B')
                        || (next_mode == 'P')) && (count == 1)) {
                    reduced_encode_mode[i] = 'u';

                } else if ((current_mode == 'D') && (next_mode == 'D') && (count == 1)) {
                    reduced_encode_mode[i] = 'u';

                } else if ((current_mode == 'D') && (next_mode == 'P') && (count <= 2)) {
                    memset(reduced_encode_mode + i, 'u', count);
                }
            }
        }

        if ((reduced_encode_mode[i] != 'p') && (reduced_encode_mode[i] != 'u') && (reduced_encode_mode[i] != 'B')) {
            current_mode = reduced_encode_mode[i];
        }
    }

    if (debug_print) {
        printf("%.*s\n", reduced_length, reduced_source);
        printf("%.*s\n", reduced_length, reduced_encode_mode);
    }

    if (bp == 0 && gs1) {
        bp = bin_append_posn(0, 5, binary_string, bp); /* P/S */
        bp = bin_append_posn(0, 5, binary_string, bp); /* FLG(n) */
        bp = bin_append_posn(0, 3, binary_string, bp); /* FLG(0) */
    }

    if (eci != 0) {
        bp = bin_append_posn(0, initial_mode == 'D' ? 4 : 5, binary_string, bp); /* P/S */
        bp = bin_append_posn(0, 5, binary_string, bp); /* FLG(n) */
        if (eci < 10) {
            bp = bin_append_posn(1, 3, binary_string, bp); /* FLG(1) */
            bp = bin_append_posn(2 + eci, 4, binary_string, bp);
        } else if (eci <= 99) {
            bp = bin_append_posn(2, 3, binary_string, bp); /* FLG(2) */
            bp = bin_append_posn(2 + (eci / 10), 4, binary_string, bp);
            bp = bin_append_posn(2 + (eci % 10), 4, binary_string, bp);
        } else if (eci <= 999) {
            bp = bin_append_posn(3, 3, binary_string, bp); /* FLG(3) */
            bp = bin_append_posn(2 + (eci / 100), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 100) / 10), 4, binary_string, bp);
            bp = bin_append_posn(2 + (eci % 10), 4, binary_string, bp);
        } else if (eci <= 9999) {
            bp = bin_append_posn(4, 3, binary_string, bp); /* FLG(4) */
            bp = bin_append_posn(2 + (eci / 1000), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 1000) / 100), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 100) / 10), 4, binary_string, bp);
            bp = bin_append_posn(2 + (eci % 10), 4, binary_string, bp);
        } else if (eci <= 99999) {
            bp = bin_append_posn(5, 3, binary_string, bp); /* FLG(5) */
            bp = bin_append_posn(2 + (eci / 10000), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 10000) / 1000), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 1000) / 100), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 100) / 10), 4, binary_string, bp);
            bp = bin_append_posn(2 + (eci % 10), 4, binary_string, bp);
        } else {
            bp = bin_append_posn(6, 3, binary_string, bp); /* FLG(6) */
            bp = bin_append_posn(2 + (eci / 100000), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 100000) / 10000), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 10000) / 1000), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 1000) / 100), 4, binary_string, bp);
            bp = bin_append_posn(2 + ((eci % 100) / 10), 4, binary_string, bp);
            bp = bin_append_posn(2 + (eci % 10), 4, binary_string, bp);
        }
    }

    current_mode = initial_mode;
    for (i = 0; i < reduced_length; i++) {

        if (reduced_encode_mode[i] != current_mode) {
            /* Change mode */
            if (current_mode == 'U') {
                switch (reduced_encode_mode[i]) {
                    case 'L':
                        if (!(bp = az_bin_append_posn(28, 5, binary_string, bp))) return 0; /* L/L */
                        break;
                    case 'M':
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        break;
                    case 'P':
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* P/L */
                        break;
                    case 'p':
                        if (!(bp = az_bin_append_posn(0, 5, binary_string, bp))) return 0; /* P/S */
                        break;
                    case 'D':
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* D/L */
                        break;
                    case 'B':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* B/S */
                        break;
                }
            } else if (current_mode == 'L') {
                switch (reduced_encode_mode[i]) {
                    case 'U':
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* D/L */
                        if (!(bp = az_bin_append_posn(14, 4, binary_string, bp))) return 0; /* U/L */
                        break;
                    case 'u':
                        if (!(bp = az_bin_append_posn(28, 5, binary_string, bp))) return 0; /* U/S */
                        break;
                    case 'M':
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        break;
                    case 'P':
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* P/L */
                        break;
                    case 'p':
                        if (!(bp = az_bin_append_posn(0, 5, binary_string, bp))) return 0; /* P/S */
                        break;
                    case 'D':
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* D/L */
                        break;
                    case 'B':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* B/S */
                        break;
                }
            } else if (current_mode == 'M') {
                switch (reduced_encode_mode[i]) {
                    case 'U':
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* U/L */
                        break;
                    case 'L':
                        if (!(bp = az_bin_append_posn(28, 5, binary_string, bp))) return 0; /* L/L */
                        break;
                    case 'P':
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* P/L */
                        break;
                    case 'p':
                        if (!(bp = az_bin_append_posn(0, 5, binary_string, bp))) return 0; /* P/S */
                        break;
                    case 'D':
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* D/L */
                        break;
                    case 'B':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* B/S */
                        break;
                }
            } else if (current_mode == 'P') {
                switch (reduced_encode_mode[i]) {
                    case 'U':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* U/L */
                        break;
                    case 'L':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(28, 5, binary_string, bp))) return 0; /* L/L */
                        break;
                    case 'M':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        break;
                    case 'D':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* D/L */
                        break;
                    case 'B':
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* U/L */
                        current_mode = 'U';
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* B/S */
                        break;
                }
            } else if (current_mode == 'D') {
                switch (reduced_encode_mode[i]) {
                    case 'U':
                        if (!(bp = az_bin_append_posn(14, 4, binary_string, bp))) return 0; /* U/L */
                        break;
                    case 'u':
                        if (!(bp = az_bin_append_posn(15, 4, binary_string, bp))) return 0; /* U/S */
                        break;
                    case 'L':
                        if (!(bp = az_bin_append_posn(14, 4, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(28, 5, binary_string, bp))) return 0; /* L/L */
                        break;
                    case 'M':
                        if (!(bp = az_bin_append_posn(14, 4, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        break;
                    case 'P':
                        if (!(bp = az_bin_append_posn(14, 4, binary_string, bp))) return 0; /* U/L */
                        if (!(bp = az_bin_append_posn(29, 5, binary_string, bp))) return 0; /* M/L */
                        if (!(bp = az_bin_append_posn(30, 5, binary_string, bp))) return 0; /* P/L */
                        break;
                    case 'p':
                        if (!(bp = az_bin_append_posn(0, 4, binary_string, bp))) return 0; /* P/S */
                        break;
                    case 'B':
                        if (!(bp = az_bin_append_posn(14, 4, binary_string, bp))) return 0; /* U/L */
                        current_mode = 'U';
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* B/S */
                        break;
                }
            }

            /* Byte mode - process full block here */
            if (reduced_encode_mode[i] == 'B') {
                int big_batch = 0;
                for (count = 0; ((i + count) < reduced_length) && (reduced_encode_mode[i + count] == 'B'); count++);

                if (count > 2047 + 2078) { /* Can't be more than 19968 / 8 = 2496 */
                    return 0;
                }

                if (count > 2047) { /* Max 11-bit number */
                    big_batch = count > 2078 ? 2078 : count;
                    /* Put 00000 followed by 11-bit number of bytes less 31 */
                    if (!(bp = az_bin_append_posn(big_batch - 31, 16, binary_string, bp))) return 0;
                    for (j = 0; j < big_batch; j++) {
                        if (!(bp = az_bin_append_posn(reduced_source[i++], 8, binary_string, bp))) return 0;
                    }
                    count -= big_batch;
                }
                if (count) {
                    if (big_batch) {
                        if (!(bp = az_bin_append_posn(31, 5, binary_string, bp))) return 0; /* B/S */
                    }
                    if (count > 31) {
                        assert(count <= 2078);
                        /* Put 00000 followed by 11-bit number of bytes less 31 */
                        if (!(bp = az_bin_append_posn(count - 31, 16, binary_string, bp))) return 0;
                    } else {
                        /* Put 5-bit number of bytes */
                        if (!(bp = az_bin_append_posn(count, 5, binary_string, bp))) return 0;
                    }
                    for (j = 0; j < count; j++) {
                        if (!(bp = az_bin_append_posn(reduced_source[i++], 8, binary_string, bp))) return 0;
                    }
                }
                i--;
                continue;
            }

            if ((reduced_encode_mode[i] != 'u') && (reduced_encode_mode[i] != 'p')) {
                current_mode = reduced_encode_mode[i];
            }
        }

        if ((reduced_encode_mode[i] == 'U') || (reduced_encode_mode[i] == 'u')) {
            if (reduced_source[i] == ' ') {
                if (!(bp = az_bin_append_posn(1, 5, binary_string, bp))) return 0; /* SP */
            } else {
                if (!(bp = az_bin_append_posn(AztecSymbolChar[reduced_source[i]], 5, binary_string, bp))) return 0;
            }
        } else if (reduced_encode_mode[i] == 'L') {
            if (reduced_source[i] == ' ') {
                if (!(bp = az_bin_append_posn(1, 5, binary_string, bp))) return 0; /* SP */
            } else {
                if (!(bp = az_bin_append_posn(AztecSymbolChar[reduced_source[i]], 5, binary_string, bp))) return 0;
            }
        } else if (reduced_encode_mode[i] == 'M') {
            if (reduced_source[i] == ' ') {
                if (!(bp = az_bin_append_posn(1, 5, binary_string, bp))) return 0; /* SP */
            } else if (reduced_source[i] == 13) {
                if (!(bp = az_bin_append_posn(14, 5, binary_string, bp))) return 0; /* CR */
            } else {
                if (!(bp = az_bin_append_posn(AztecSymbolChar[reduced_source[i]], 5, binary_string, bp))) return 0;
            }
        } else if ((reduced_encode_mode[i] == 'P') || (reduced_encode_mode[i] == 'p')) {
            if (gs1 && reduced_source[i] == '\x1D') {
                if (!(bp = az_bin_append_posn(0, 5, binary_string, bp))) return 0; /* FLG(n) */
                if (!(bp = az_bin_append_posn(0, 3, binary_string, bp))) return 0; /* FLG(0) = FNC1 */
            } else if (reduced_source[i] == 13) {
                if (!(bp = az_bin_append_posn(1, 5, binary_string, bp))) return 0; /* CR */
            } else if (reduced_source[i] == 'a') {
                if (!(bp = az_bin_append_posn(2, 5, binary_string, bp))) return 0; /* CR LF */
            } else if (reduced_source[i] == 'b') {
                if (!(bp = az_bin_append_posn(3, 5, binary_string, bp))) return 0; /* . SP */
            } else if (reduced_source[i] == 'c') {
                if (!(bp = az_bin_append_posn(4, 5, binary_string, bp))) return 0; /* , SP */
            } else if (reduced_source[i] == 'd') {
                if (!(bp = az_bin_append_posn(5, 5, binary_string, bp))) return 0; /* : SP */
            } else if (reduced_source[i] == ',') {
                if (!(bp = az_bin_append_posn(17, 5, binary_string, bp))) return 0; /* Comma */
            } else if (reduced_source[i] == '.') {
                if (!(bp = az_bin_append_posn(19, 5, binary_string, bp))) return 0; /* Full stop */
            } else {
                if (!(bp = az_bin_append_posn(AztecSymbolChar[reduced_source[i]], 5, binary_string, bp))) return 0;
            }
        } else if (reduced_encode_mode[i] == 'D') {
            if (reduced_source[i] == ' ') {
                if (!(bp = az_bin_append_posn(1, 4, binary_string, bp))) return 0; /* SP */
            } else if (reduced_source[i] == ',') {
                if (!(bp = az_bin_append_posn(12, 4, binary_string, bp))) return 0; /* Comma */
            } else if (reduced_source[i] == '.') {
                if (!(bp = az_bin_append_posn(13, 4, binary_string, bp))) return 0; /* Full stop */
            } else {
                if (!(bp = az_bin_append_posn(AztecSymbolChar[reduced_source[i]], 4, binary_string, bp))) return 0;
            }
        }
    }

    if (debug_print) {
        printf("Binary String (%d): %.*s\n", bp, bp, binary_string);
    }

    *data_length = bp;
    if (p_current_mode) {
        *p_current_mode = current_mode;
    }

    return 1;
}

/* Call `aztec_text_process()` for each segment */
static int aztec_text_process_segs(struct zint_seg segs[], const int seg_count, int bp, char binary_string[],
            const int gs1, int *data_length, const int debug_print) {
    int i;

    char current_mode = 'U';

    for (i = 0; i < seg_count; i++) {
        if (!aztec_text_process(segs[i].source, segs[i].length, bp, binary_string, gs1, segs[i].eci, &current_mode,
                &bp, debug_print)) {
            return 0;
        }
    }

    *data_length = bp;

    return 1;
}

/* Prevent data from obscuring reference grid */
static int az_avoidReferenceGrid(int output) {

    if (output > 10) {
        output += (output - 11) / 15 + 1;
    }

    return output;
}

/* Calculate the position of the bits in the grid (non-compact) */
static void az_populate_map(short AztecMap[], const int layers) {
    int layer;
    int x, y;
    const int offset = AztecOffset[layers - 1];
    const int endoffset = 151 - offset;

    for (layer = 0; layer < layers; layer++) {
        const int start = (112 * layer) + (16 * layer * layer) + 2;
        const int length = 28 + (layer * 4) + (layer + 1) * 4;
        int av0, av1;
        int n = start, end;
        /* Top */
        x = 64 - (layer * 2);
        y = 63 - (layer * 2);
        av0 = az_avoidReferenceGrid(y) * 151;
        av1 = az_avoidReferenceGrid(y - 1) * 151;
        end = start + length;
        while (n < end) {
            const int avxi = az_avoidReferenceGrid(x++);
            AztecMap[av0 + avxi] = n++;
            AztecMap[av1 + avxi] = n++;
        }
        /* Right */
        x = 78 + (layer * 2);
        y = 64 - (layer * 2);
        av0 = az_avoidReferenceGrid(x);
        av1 = az_avoidReferenceGrid(x + 1);
        end += length;
        while (n < end) {
            const int avyi = az_avoidReferenceGrid(y++) * 151;
            AztecMap[avyi + av0] = n++;
            AztecMap[avyi + av1] = n++;
        }
        /* Bottom */
        x = 77 + (layer * 2);
        y = 78 + (layer * 2);
        av0 = az_avoidReferenceGrid(y) * 151;
        av1 = az_avoidReferenceGrid(y + 1) * 151;
        end += length;
        while (n < end) {
            const int avxi = az_avoidReferenceGrid(x--);
            AztecMap[av0 + avxi] = n++;
            AztecMap[av1 + avxi] = n++;
        }
        /* Left */
        x = 63 - (layer * 2);
        y = 77 + (layer * 2);
        av0 = az_avoidReferenceGrid(x);
        av1 = az_avoidReferenceGrid(x - 1);
        end += length;
        while (n < end) {
            const int avyi = az_avoidReferenceGrid(y--) * 151;
            AztecMap[avyi + av0] = n++;
            AztecMap[avyi + av1] = n++;
        }
    }

    /* Copy "Core Symbol" (finder, descriptor, orientation) */
    for (y = 0; y < 15; y++) {
        memcpy(AztecMap + (y + 68) * 151 + 68, AztecMapCore[y], sizeof(short) * 15);
    }

    /* Reference grid guide bars */
    for (y = offset <= 11 ? 11 : AztecMapGridYOffsets[(offset - 11) / 16]; y < endoffset; y += 16) {
        for (x = offset; x < endoffset; x++) {
            AztecMap[(x * 151) + y] = x & 1;
            AztecMap[(y * 151) + x] = x & 1;
        }
    }
}

/* Helper to insert dummy '0' or '1's into runs of same bits. See ISO/IEC 24778:2008 7.3.1.2 */
static int az_bitrun_stuff(const char *binary_string, const int data_length, const int codeword_size,
            const int data_maxsize, char adjusted_string[AZTEC_MAX_CAPACITY]) {
    int i, j = 0, count = 0;

    for (i = 0; i < data_length; i++) {

        if ((j + 1) % codeword_size == 0) {
            /* Last bit of codeword */
            /* 7.3.1.2 "whenever the first B-1 bits ... are all “0”s, then a dummy “1” is inserted..."
               "Similarly a message codeword that starts with B-1 “1”s has a dummy “0” inserted..." */

            if (count == 0 || count == (codeword_size - 1)) {
                /* Codeword of B-1 '0's or B-1 '1's */
                if (j > data_maxsize) {
                    return 0; /* Fail */
                }
                adjusted_string[j++] = count == 0 ? '1' : '0';
                count = binary_string[i] == '1' ? 1 : 0;
            } else {
                count = 0;
            }

        } else if (binary_string[i] == '1') { /* Skip B so only counting B-1 */
            count++;
        }
        if (j > data_maxsize) {
            return 0; /* Fail */
        }
        adjusted_string[j++] = binary_string[i];
    }

    return j;
}

/* Helper to add padding, accounting for bitrun stuffing */
static int az_add_padding(const int padbits, const int codeword_size, char adjusted_string[AZTEC_MAX_CAPACITY],
            int adjusted_length) {
    int i, count = 0;

    for (i = 0; i < padbits; i++) {
        adjusted_string[adjusted_length++] = '1';
    }

    for (i = (adjusted_length - codeword_size); i < adjusted_length; i++) {
        count += adjusted_string[i] == '1';
    }
    if (count == codeword_size) {
        adjusted_string[adjusted_length - 1] = '0';
    }

    return adjusted_length;
}

/* Determine codeword bitlength - Table 3 */
static int az_codeword_size(const int layers) {
    int codeword_size;

    if (layers <= 2) {
        codeword_size = 6;
    } else if (layers <= 8) {
        codeword_size = 8;
    } else if (layers <= 22) {
        codeword_size = 10;
    } else {
        codeword_size = 12;
    }
    return codeword_size;
}

INTERNAL int aztec(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int x, y, i, p, data_blocks, ecc_blocks, layers, total_bits;
    char bit_pattern[AZTEC_MAP_POSN_MAX + 1]; /* Note AZTEC_MAP_POSN_MAX > AZTEC_BIN_CAPACITY */
    /* To lessen stack usage, share binary_string buffer with bit_pattern, as accessed separately */
    char *binary_string = bit_pattern;
    char descriptor[42];
    char adjusted_string[AZTEC_MAX_CAPACITY];
    short AztecMap[AZTEC_MAP_SIZE];
    unsigned char desc_data[4], desc_ecc[6];
    int error_number = 0;
    int compact, data_length, data_maxsize, codeword_size, adjusted_length;
    int remainder, padbits, adjustment_size;
    int bp = 0;
    const int gs1 = (symbol->input_mode & 0x07) == GS1_MODE;
    const int reader_init = symbol->output_options & READER_INIT;
    const int compact_loop_start = reader_init ? 1 : 4; /* Compact 2-4 excluded from Reader Initialisation */
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    rs_t rs;
    rs_uint_t rs_uint;
    unsigned int *data_part;
    unsigned int *ecc_part;

    if (gs1 && reader_init) {
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 501, "Cannot use Reader Initialisation in GS1 mode");
    }

    if (symbol->structapp.count) {
        /* Structured Append info as string <SP> + ID + <SP> + index + count + NUL */
        unsigned char sa_src[1 + sizeof(symbol->structapp.id) + 1 + 1 + 1 + 1] = {0};
        int sa_len;
        int id_len;

        if (symbol->structapp.count < 2 || symbol->structapp.count > 26) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 701,
                            "Structured Append count '%d' out of range (2 to 26)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 702,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }

        for (id_len = 0; id_len < 32 && symbol->structapp.id[id_len]; id_len++);

        if (id_len && chr_cnt((const unsigned char *) symbol->structapp.id, id_len, ' ')) {
            /* Note ID can contain any old chars apart from space so don't print in error message */
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 703, "Structured Append ID cannot contain spaces");
        }

        bp = bin_append_posn(29, 5, binary_string, bp); /* M/L */
        bp = bin_append_posn(29, 5, binary_string, bp); /* U/L */

        sa_len = 0;
        if (id_len) { /* ID has a space on either side */
            sa_src[sa_len++] = ' ';
            memcpy(sa_src + sa_len, symbol->structapp.id, id_len);
            sa_len += id_len;
            sa_src[sa_len++] = ' ';
        }
        sa_src[sa_len++] = 'A' + symbol->structapp.index - 1;
        sa_src[sa_len++] = 'A' + symbol->structapp.count - 1;
        if (debug_print) {
            printf("Structured Append Count: %d, Index: %d, ID: %.32s, String: %s\n",
                    symbol->structapp.count, symbol->structapp.index, symbol->structapp.id, sa_src);
        }

        (void) aztec_text_process(sa_src, sa_len, bp, binary_string, 0 /*gs1*/, 0 /*eci*/, NULL /*p_current_mode*/,
                                    &bp, debug_print);
        /* Will be in U/L due to uppercase A-Z index/count indicators at end */
    }

    if (!aztec_text_process_segs(segs, seg_count, bp, binary_string, gs1, &data_length, debug_print)) {
        return errtxt(ZINT_ERROR_TOO_LONG, symbol, 502,
                        "Input too long, requires too many codewords (maximum " AZ_BIN_CAP_CWDS_S ")");
    }
    assert(data_length > 0); /* Suppress clang-tidy warning: clang-analyzer-core.UndefinedBinaryOperatorResult */

    if (symbol->option_1 < -1 || symbol->option_1 > 4) {
        errtxtf(0, symbol, 503, "Error correction level '%d' out of range (1 to 4)", symbol->option_1);
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        error_number = errtxt_adj(ZINT_WARN_INVALID_OPTION, symbol, "%1$s%2$s", ", ignoring");
        symbol->option_1 = -1;
    }

    data_maxsize = 0; /* Keep compiler happy! */
    adjustment_size = 0;
    if (symbol->option_2 == 0) { /* The size of the symbol can be determined by Zint */
        int ecc_level = symbol->option_1;

        if (ecc_level <= 0) {
            ecc_level = 2;
        }

        do {
            /* Decide what size symbol to use - the smallest that fits the data */
            compact = 0; /* 1 = Aztec Compact, 0 = Normal Aztec */
            layers = 0;

            /* For each level of error correction work out the smallest symbol which the data will fit in */
            for (i = compact_loop_start; i > 0; i--) {
                if ((data_length + adjustment_size) <= AztecCompactDataSizes[ecc_level - 1][i - 1]) {
                    layers = i;
                    compact = 1;
                    data_maxsize = AztecCompactDataSizes[ecc_level - 1][i - 1];
                }
            }
            if (!compact) {
                for (i = 32; i > 0; i--) {
                    if ((data_length + adjustment_size) <= AztecDataSizes[ecc_level - 1][i - 1]) {
                        layers = i;
                        compact = 0;
                        data_maxsize = AztecDataSizes[ecc_level - 1][i - 1];
                    }
                }
            }

            if (layers == 0) { /* Couldn't find a symbol which fits the data */
                if (adjustment_size == 0) {
                    return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 707,
                                    "Input too long for ECC level %1$d, requires too many codewords (maximum %2$d)",
                                    ecc_level, AztecDataSizes[ecc_level - 1][31] / 12);
                }
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 504,
                                "Input too long for ECC level %1$d, requires %2$d codewords (maximum %3$d)",
                                ecc_level, (data_length + adjustment_size + 11) / 12,
                                AztecDataSizes[ecc_level - 1][31] / 12);
            }

            codeword_size = az_codeword_size(layers);

            adjusted_length = az_bitrun_stuff(binary_string, data_length, codeword_size,
                                                adjustment_size ? data_maxsize : AZTEC_BIN_CAPACITY, adjusted_string);
            if (adjusted_length == 0) {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 705,
                                "Input too long for ECC level %1$d, requires too many codewords (maximum %2$d)",
                                ecc_level, (adjustment_size ? data_maxsize : AZTEC_BIN_CAPACITY) / codeword_size);
            }
            adjustment_size = adjusted_length - data_length;

            /* Add padding */
            remainder = adjusted_length % codeword_size;

            padbits = codeword_size - remainder;
            if (padbits == codeword_size) {
                padbits = 0;
            }
            if (debug_print) printf("Remainder: %d  Pad bits: %d\n", remainder, padbits);

            assert(adjusted_length <= AZTEC_BIN_CAPACITY);

            adjusted_length = az_add_padding(padbits, codeword_size, adjusted_string, adjusted_length);

            if (debug_print) printf("Adjusted Length: %d, Data Max Size %d\n", adjusted_length, data_maxsize);

        } while (adjusted_length > data_maxsize);
        /* This loop will only repeat on the rare occasions when the rule about not having all 1s or all 0s
        means that the binary string has had to be lengthened beyond the maximum number of bits that can
        be encoded in a symbol of the selected size */

    } else { /* The size of the symbol has been specified by the user */
        if ((symbol->option_2 < 0) || (symbol->option_2 > 36)) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 510, "Version '%d' out of range (1 to 36)",
                            symbol->option_2);
        }
        if (reader_init) {
            /* For back-compatibility, silently ignore compact 2-4 requests but error on layers > 22 */
            if (symbol->option_2 >= 2 && symbol->option_2 <= 4) {
                symbol->option_2 = 5;
            } else if (symbol->option_2 > 26) {
                /* Caught below anyway but catch here also for better feedback */
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 709,
                                "Version '%d' out of range for Reader Initialisation symbols (maximum 26)",
                                symbol->option_2);
            }
        }
        if (symbol->option_2 <= 4) {
            compact = 1;
            layers = symbol->option_2;
        } else {
            compact = 0;
            layers = symbol->option_2 - 4;
        }

        codeword_size = az_codeword_size(layers);
        if (compact) {
            data_maxsize = codeword_size * (AztecCompactSizes[layers - 1] - 3);
        } else {
            data_maxsize = codeword_size * (AztecSizes[layers - 1] - 3);
        }

        adjusted_length = az_bitrun_stuff(binary_string, data_length, codeword_size, data_maxsize, adjusted_string);
        if (adjusted_length == 0) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 704,
                            "Input too long for Version %1$d, requires too many codewords (maximum %2$d)",
                            symbol->option_2, data_maxsize / codeword_size);
        }

        /* Add padding */
        remainder = adjusted_length % codeword_size;

        padbits = codeword_size - remainder;
        if (padbits == codeword_size) {
            padbits = 0;
        }
        if (debug_print) printf("Remainder: %d  Pad bits: %d\n", remainder, padbits);

        /* Check if the data actually fits into the selected symbol size */

        if (adjusted_length + padbits > data_maxsize) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 505,
                            "Input too long for Version %1$d, requires %2$d codewords (maximum %3$d)",
                            symbol->option_2, (adjusted_length + padbits) / codeword_size,
                            data_maxsize / codeword_size);
        }

        adjusted_length = az_add_padding(padbits, codeword_size, adjusted_string, adjusted_length);

        if (debug_print) printf("Adjusted Length: %d\n", adjusted_length);
    }

    if (debug_print) {
        printf("Codewords (%d):\n", adjusted_length / codeword_size);
        for (i = 0; i < (adjusted_length / codeword_size); i++) {
            printf(" %.*s", codeword_size, adjusted_string + i * codeword_size);
        }
        fputc('\n', stdout);
    }

    if (reader_init && (layers > 22)) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 506,
                        "Input too long for Reader Initialisation, requires %d layers (maximum 22)", layers);
    }

    data_blocks = adjusted_length / codeword_size;

    if (compact) {
        ecc_blocks = AztecCompactSizes[layers - 1] - data_blocks;
        if (layers == 4) { /* Can use spare blocks for ECC (76 available - 64 max data blocks) */
            ecc_blocks += 12;
        }
    } else {
        ecc_blocks = AztecSizes[layers - 1] - data_blocks;
    }
    if (ecc_blocks < data_blocks / 20) {
        error_number = errtxtf(ZINT_WARN_NONCOMPLIANT, symbol, 708,
                                "Number of ECC codewords %1$d less than %2$d (5%% of data codewords %3$d)",
                                ecc_blocks, data_blocks / 20, data_blocks);
    }

    if (debug_print) {
        printf("Generating a %s symbol with %d layers\n", compact ? "compact" : "full-size", layers);
        printf("Requires %d codewords of %d-bits\n", data_blocks + ecc_blocks, codeword_size);
        printf("    (%d data words, %d ecc words)\n", data_blocks, ecc_blocks);
    }

    data_part = (unsigned int *) z_alloca(sizeof(unsigned int) * data_blocks);
    ecc_part = (unsigned int *) z_alloca(sizeof(unsigned int) * ecc_blocks);

    /* Copy across data into separate integers */
    memset(data_part, 0, sizeof(unsigned int) * data_blocks);
    memset(ecc_part, 0, sizeof(unsigned int) * ecc_blocks);

    /* Split into codewords and calculate reed-solomon error correction codes */
    for (i = 0; i < data_blocks; i++) {
        for (p = 0; p < codeword_size; p++) {
            if (adjusted_string[i * codeword_size + p] == '1') {
                data_part[i] |= 0x01 << (codeword_size - (p + 1));
            }
        }
    }

    switch (codeword_size) {
        case 6:
            rs_init_gf(&rs, 0x43);
            rs_init_code(&rs, ecc_blocks, 1);
            rs_encode_uint(&rs, data_blocks, data_part, ecc_part);
            break;
        case 8:
            rs_init_gf(&rs, 0x12d);
            rs_init_code(&rs, ecc_blocks, 1);
            rs_encode_uint(&rs, data_blocks, data_part, ecc_part);
            break;
        case 10:
            if (!rs_uint_init_gf(&rs_uint, 0x409, 1023)) { /* Can fail on malloc() */
                return errtxt(ZINT_ERROR_MEMORY, symbol, 500, "Insufficient memory for Reed-Solomon log tables");
            }
            rs_uint_init_code(&rs_uint, ecc_blocks, 1);
            rs_uint_encode(&rs_uint, data_blocks, data_part, ecc_part);
            rs_uint_free(&rs_uint);
            break;
        case 12:
            if (!rs_uint_init_gf(&rs_uint, 0x1069, 4095)) { /* Can fail on malloc() */
                /* Note using AUSPOST error nos range as out of 50x ones & 51x taken by CODEONE */
                return errtxt(ZINT_ERROR_MEMORY, symbol, 700, "Insufficient memory for Reed-Solomon log tables");
            }
            rs_uint_init_code(&rs_uint, ecc_blocks, 1);
            rs_uint_encode(&rs_uint, data_blocks, data_part, ecc_part);
            rs_uint_free(&rs_uint);
            break;
    }

    for (i = 0; i < ecc_blocks; i++) {
        adjusted_length = bin_append_posn(ecc_part[i], codeword_size, adjusted_string, adjusted_length);
    }

    /* Invert the data so that actual data is on the outside and reed-solomon on the inside */
    memset(bit_pattern, '0', AZTEC_MAP_POSN_MAX + 1);

    total_bits = (data_blocks + ecc_blocks) * codeword_size;
    for (i = 0; i < total_bits; i++) {
        bit_pattern[i] = adjusted_string[total_bits - i - 1];
    }

    /* Now add the symbol descriptor */
    memset(desc_data, 0, 4);
    memset(desc_ecc, 0, 6);
    memset(descriptor, 0, 42);

    if (compact) {
        /* The first 2 bits represent the number of layers minus 1 */
        descriptor[0] = ((layers - 1) & 0x02) ? '1' : '0';
        descriptor[1] = ((layers - 1) & 0x01) ? '1' : '0';

        /* The next 6 bits represent the number of data blocks minus 1 */
        descriptor[2] = reader_init || ((data_blocks - 1) & 0x20) ? '1' : '0';
        for (i = 3; i < 8; i++) {
            descriptor[i] = ((data_blocks - 1) & (0x10 >> (i - 3))) ? '1' : '0';
        }
        if (debug_print) printf("Mode Message = %.8s\n", descriptor);
    } else {
        /* The first 5 bits represent the number of layers minus 1 */
        for (i = 0; i < 5; i++) {
            descriptor[i] = ((layers - 1) & (0x10 >> i)) ? '1' : '0';
        }

        /* The next 11 bits represent the number of data blocks minus 1 */
        descriptor[5] = reader_init || ((data_blocks - 1) & 0x400) ? '1' : '0';
        for (i = 6; i < 16; i++) {
            descriptor[i] = ((data_blocks - 1) & (0x200 >> (i - 6))) ? '1' : '0';
        }
        if (debug_print) printf("Mode Message = %.16s\n", descriptor);
    }

    /* Split into 4-bit codewords */
    for (i = 0; i < 4; i++) {
        desc_data[i] = ((descriptor[i * 4] == '1') << 3) | ((descriptor[(i * 4) + 1] == '1') << 2)
                        | ((descriptor[(i * 4) + 2] == '1') << 1) | (descriptor[(i * 4) + 3] == '1');
    }

    /* Add Reed-Solomon error correction with Galois field GF(16) and prime modulus x^4 + x + 1 (section 7.2.3) */

    rs_init_gf(&rs, 0x13);
    if (compact) {
        rs_init_code(&rs, 5, 1);
        rs_encode(&rs, 2, desc_data, desc_ecc);
        for (i = 0; i < 5; i++) {
            descriptor[(i * 4) + 8] = (desc_ecc[i] & 0x08) ? '1' : '0';
            descriptor[(i * 4) + 9] = (desc_ecc[i] & 0x04) ? '1' : '0';
            descriptor[(i * 4) + 10] = (desc_ecc[i] & 0x02) ? '1' : '0';
            descriptor[(i * 4) + 11] = (desc_ecc[i] & 0x01) ? '1' : '0';
        }
    } else {
        rs_init_code(&rs, 6, 1);
        rs_encode(&rs, 4, desc_data, desc_ecc);
        for (i = 0; i < 6; i++) {
            descriptor[(i * 4) + 16] = (desc_ecc[i] & 0x08) ? '1' : '0';
            descriptor[(i * 4) + 17] = (desc_ecc[i] & 0x04) ? '1' : '0';
            descriptor[(i * 4) + 18] = (desc_ecc[i] & 0x02) ? '1' : '0';
            descriptor[(i * 4) + 19] = (desc_ecc[i] & 0x01) ? '1' : '0';
        }
    }

    /* Merge descriptor with the rest of the symbol */
    if (compact) {
        memcpy(bit_pattern + 2000 - 2, descriptor, 40);
    } else {
        memcpy(bit_pattern + 20000 - 2, descriptor, 40);
    }

    /* Plot all of the data into the symbol in pre-defined spiral pattern */
    if (compact) {
        const int offset = AztecCompactOffset[layers - 1];
        const int end_offset = 27 - offset;
        for (y = offset; y < end_offset; y++) {
            const int y_map = y * 27;
            for (x = offset; x < end_offset; x++) {
                const int map = AztecCompactMap[y_map + x];
                if (map == 1 || (map >= 2 && bit_pattern[map - 2] == '1')) {
                    set_module(symbol, y - offset, x - offset);
                }
            }
            symbol->row_height[y - offset] = 1;
        }
        symbol->height = 27 - (2 * offset);
        symbol->rows = 27 - (2 * offset);
        symbol->width = 27 - (2 * offset);
    } else {
        const int offset = AztecOffset[layers - 1];
        const int end_offset = 151 - offset;
        az_populate_map(AztecMap, layers);
        for (y = offset; y < end_offset; y++) {
            const int y_map = y * 151;
            for (x = offset; x < end_offset; x++) {
                const int map = AztecMap[y_map + x];
                if (map == 1 || (map >= 2 && bit_pattern[map - 2] == '1')) {
                    set_module(symbol, y - offset, x - offset);
                }
            }
            symbol->row_height[y - offset] = 1;
        }
        symbol->height = 151 - (2 * offset);
        symbol->rows = 151 - (2 * offset);
        symbol->width = 151 - (2 * offset);
    }

    return error_number;
}

/* Encodes Aztec runes as specified in ISO/IEC 24778:2008 Annex A */
INTERNAL int azrune(struct zint_symbol *symbol, unsigned char source[], int length) {
    unsigned int input_value;
    int i, y, x, r;
    char binary_string[28];
    unsigned char data_codewords[3], ecc_codewords[6];
    int bp = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    rs_t rs;

    input_value = 0;
    if (length > 3) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 507, "Input length %d too long (maximum 3)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 508,
                        "Invalid character at position %d in input (digits only)", i);
    }
    switch (length) {
        case 3:
            input_value = 100 * ctoi(source[0]) + 10 * ctoi(source[1]) + ctoi(source[2]);
            break;
        case 2:
            input_value = 10 * ctoi(source[0]) + ctoi(source[1]);
            break;
        case 1:
            input_value = ctoi(source[0]);
            break;
    }

    if (input_value > 255) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 509, "Input value out of range (0 to 255)");
    }

    bp = bin_append_posn(input_value, 8, binary_string, bp);

    data_codewords[0] = (unsigned char) (input_value >> 4);
    data_codewords[1] = (unsigned char) (input_value & 0xF);

    rs_init_gf(&rs, 0x13);
    rs_init_code(&rs, 5, 1);
    rs_encode(&rs, 2, data_codewords, ecc_codewords);

    for (i = 0; i < 5; i++) {
        bp = bin_append_posn(ecc_codewords[i], 4, binary_string, bp);
    }

    for (i = 0; i < 28; i += 2) {
        binary_string[i] = '0' + (binary_string[i] != '1');
    }

    if (debug_print) {
        printf("Binary String: %.28s\n", binary_string);
    }

    for (y = 8; y < 19; y++) {
        r = y * 27;
        for (x = 8; x < 19; x++) {
            if (AztecCompactMap[r + x] == 1) {
                set_module(symbol, y - 8, x - 8);
            } else if (AztecCompactMap[r + x] && binary_string[AztecCompactMap[r + x] - 2000] == '1') {
                set_module(symbol, y - 8, x - 8);
            }
        }
        symbol->row_height[y - 8] = 1;
    }
    symbol->height = 11;
    symbol->rows = 11;
    symbol->width = 11;

    return 0;
}

/* vim: set ts=4 sw=4 et : */
