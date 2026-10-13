/* gs1.c - Verifies GS1 data */
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
#include "gs1.h"

/* gs1_lint() validators and checkers */

/* Validate numeric */
static int numeric(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50]) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de; d++) {
            if (!z_isdigit(*d)) {
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                sprintf(err_msg, "Non-numeric character '%c'", *d);
                return 0;
            }
        }
    }

    return 1;
}

/* GS1 General Specifications 21.0.1 Figure 7.9.5-1. GS1 AI encodable character reference values.
   Also used to determine if character in set 82 - a value of 82 means not in */
static const char c82[] = {
     0,  1, 82, 82,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, /*!-0*/
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 82, /*1-@*/
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, /*A-P*/
    45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 82, 82, 82, 82, 55, 82, /*Q-`*/
    56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, /*a-p*/
    72, 73, 74, 75, 76, 77, 78, 79, 80, 81, /*q-z*/
};

/* Validate of character set 82 (GS1 General Specifications Figure 7.11-1) */
static int cset82(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50]) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de; d++) {
            if (*d < '!' || *d > 'z' || c82[*d - '!'] == 82) {
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                sprintf(err_msg, "Invalid CSET 82 character '%c'", *d);
                return 0;
            }
        }
    }

    return 1;
}

/* Validate of character set 39 (GS1 General Specifications Figure 7.11-2) */
static int cset39(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50]) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de; d++) {
            /* 0-9, A-Z and "#", "-", "/" */
            if ((*d < '0' && *d != '#' && *d != '-' && *d != '/') || (*d > '9' && *d < 'A') || *d > 'Z') {
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                sprintf(err_msg, "Invalid CSET 39 character '%c'", *d);
                return 0;
            }
        }
    }

    return 1;
}

/* Validate of character set 64 (GSCN 21-307 Figure 7.11-3) */
static int cset64(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50]) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de; d++) {
            /* 0-9, A-Z, a-z and "-", "_" */
            if ((*d < '0' && *d != '-') || (*d > '9' && *d < 'A') || (*d > 'Z' && *d < 'a' && *d != '_')
                    || *d > 'z') {
                /* One or two "="s can be used as padding to mod 3 length */
                if (*d == '=' && (d + 1 == de || (d + 2 == de && *(d + 1) == '=')) && data_len % 3 == 0) {
                    break;
                }
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                sprintf(err_msg, "Invalid CSET 64 character '%c'", *d);
                return 0;
            }
        }
    }

    return 1;
}

/* Check a check digit (GS1 General Specifications 7.9.1) */
static int csum(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len) - 1; /* Note less last character */
        int checksum = 0;
        int factor = (min & 1) ? 1 : 3;

        for (; d < de; d++) {
            checksum += (*d - '0') * factor;
            factor ^= 0x02; /* Toggles 1 and 3 */
        }
        checksum = 10 - checksum % 10;
        if (checksum == 10) {
            checksum = 0;
        }
        if (checksum != *d - '0') {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            sprintf(err_msg, "Bad checksum '%c', expected '%c'", *d, checksum + '0');
            return 0;
        }
    }

    return 1;
}

/* Check alphanumeric check characters (GS1 General Specifications 7.9.5) */
static int csumalpha(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }
    /* Do this check separately for backward compatibility */
    if (data_len && data_len < 2) {
        *p_err_no = 4;
        return 0;
    }

    if (!length_only && data_len) {
        static const char c32[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
        static const char weights[] = {
            2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83
        };
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len) - 2; /* Note less last 2 characters */
        int checksum = 0, c1, c2;

        for (; d < de; d++) {
            checksum += c82[*d - '!'] * weights[de - 1 - d];
        }
        checksum %= 1021;
        c1 = c32[checksum >> 5];
        c2 = c32[checksum & 0x1F];

        if (de[0] != c1 || de[1] != c2) {
            *p_err_no = 3;
            if (de[0] != c1) {
                *p_err_posn = (de - data) + 1;
                sprintf(err_msg, "Bad checksum '%c', expected '%c'", de[0], c1);
            } else {
                *p_err_posn = (de + 1 - data) + 1;
                sprintf(err_msg, "Bad checksum '%c', expected '%c'", de[1], c2);
            }
            return 0;
        }
    }

    return 1;
}

/* Check for a GS1 Prefix (GS1 General Specifications GS1 1.4.2) */
static int key(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }
    /* Do this check separately for backward compatibility */
    if (data_len && data_len < 2) {
        *p_err_no = 4;
        return 0;
    }

    if (!length_only && data_len) {
        data += offset;

        if (!z_isdigit(data[0]) || !z_isdigit(data[1])) {
            *p_err_no = 3;
            *p_err_posn = offset + z_isdigit(data[0]) + 1;
            sprintf(err_msg, "Non-numeric company prefix '%c'", data[z_isdigit(data[0])]);
            return 0;
        }
    }

    return 1;
}

/* Note following date/time checkers (!length_only) assume data all digits, i.e. `numeric()` has succeeded */

/* Check for a date YYYYMMDD with zero day allowed */
static int yyyymmd0(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    static const char days_in_month[13] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 8)) {
        return 0;
    }

    if (!length_only && data_len) {
        int month, day;

        month = to_int(data + offset + 4, 2);
        if (month == 0 || month > 12) {
            *p_err_no = 3;
            *p_err_posn = offset + 4 + 1;
            sprintf(err_msg, "Invalid month '%.2s'", data + offset + 4);
            return 0;
        }

        day = to_int(data + offset + 6, 2);
        if (day && day > days_in_month[month]) {
            *p_err_no = 3;
            *p_err_posn = offset + 6 + 1;
            sprintf(err_msg, "Invalid day '%.2s'", data + offset + 6);
            return 0;
        }
        /* Leap year check */
        if (month == 2 && day == 29) {
            const int year = to_int(data + offset, 4);
            if ((year & 3) || (year % 100 == 0 && year % 400 != 0)) {
                *p_err_no = 3;
                *p_err_posn = offset + 6 + 1;
                sprintf(err_msg, "Invalid day '%.2s'", data + offset + 6);
                return 0;
            }
        }
    }

    return 1;
}

/* Check for a date YYYYMMDD. Zero day NOT allowed */
static int yyyymmdd(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    if (!yyyymmd0(data, data_len, offset, min, max, p_err_no, p_err_posn, err_msg, length_only)) {
        return 0;
    }

    data_len = data_len < offset ? 0 : data_len - offset;

    if (!length_only && data_len) {
        const int day = to_int(data + offset + 6, 2);
        if (day == 0) {
            *p_err_no = 3;
            *p_err_posn = offset + 6 + 1;
            sprintf(err_msg, "Invalid day '%.2s'", data + offset + 6);
            return 0;
        }
    }

    return 1;
}

/* Check for a date YYMMDD with zero day allowed */
static int yymmd0(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 6)) {
        return 0;
    }

    if (!length_only && data_len) {
        /* For leap year detection, only matters if 00 represents century divisible by 400 or not */
        /* Following good until 2050 when 00 will mean 2100 (GS1 General Specifications 7.12) */
        unsigned char buf[8] = { '2', '0' };

        memcpy(buf + 2, data + offset, 6);
        if (!yyyymmd0(buf, 8, 0, min, max, p_err_no, p_err_posn, err_msg, length_only)) {
            *p_err_posn += offset - 2;
            return 0;
        }
    }

    return 1;
}

/* Check for a date YYMMDD. Zero day NOT allowed */
static int yymmdd(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    if (!yymmd0(data, data_len, offset, min, max, p_err_no, p_err_posn, err_msg, length_only)) {
        return 0;
    }

    data_len = data_len < offset ? 0 : data_len - offset;

    if (!length_only && data_len) {
        const int day = to_int(data + offset + 4, 2);
        if (day == 0) {
            *p_err_no = 3;
            *p_err_posn = offset + 4 + 1;
            sprintf(err_msg, "Invalid day '%.2s'", data + offset + 4);
            return 0;
        }
    }

    return 1;
}

/* Check for a time HHMI */
static int hhmi(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 4)) {
        return 0;
    }

    if (!length_only && data_len) {
        int hour, mins;

        hour = to_int(data + offset, 2);
        if (hour > 23) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid hour of day '%.2s'", data + offset);
            return 0;
        }
        mins = to_int(data + offset + 2, 2);
        if (mins > 59) {
            *p_err_no = 3;
            *p_err_posn = offset + 2 + 1;
            sprintf(err_msg, "Invalid minutes in the hour '%.2s'", data + offset + 2);
            return 0;
        }
    }

    return 1;
}

/* Check for a time HH (hours) */
static int hh(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 2)) {
        return 0;
    }

    if (!length_only && data_len) {
        const int hour = to_int(data + offset, 2);
        if (hour > 23) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid hour of day '%.2s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Check for a time MI (minutes) */
static int mi(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 2)) {
        return 0;
    }

    if (!length_only && data_len) {
        const int mins = to_int(data + offset, 2);
        if (mins > 59) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid minutes in the hour '%.2s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Check for a time SS (seconds) */
static int ss(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 2)) {
        return 0;
    }

    if (!length_only && data_len) {
        const int secs = to_int(data + offset, 2);
        if (secs > 59) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid seconds in the minute '%.2s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Generated by "php backend/tools/gen_iso3166_h.php > backend/iso3166.h" */
#include "iso3166.h"

/* Check for an ISO 3166-1 numeric country code */
static int iso3166(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 3)) {
        if (offset) {
            /* For backward compatibility only warn if not first */
            *p_err_no = 4;
        }
        return 0;
    }

    if (!length_only && data_len) {
        if (!iso3166_numeric(to_int(data + offset, 3))) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Unknown country code '%.3s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Check for an ISO 3166-1 numeric country code allowing "999" */
static int iso3166999(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 3)) {
        return 0;
    }

    if (!length_only && data_len) {
        const int cc = to_int(data + offset, 3);
        if (cc != 999 && !iso3166_numeric(cc)) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Unknown country code '%.3s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Check for an ISO 3166-1 alpha2 country code */
static int iso3166alpha2(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 2)) {
        return 0;
    }

    if (!length_only && data_len) {
        if (!iso3166_alpha2((const char *) (data + offset))) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Unknown country code '%.2s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Generated by "php backend/tools/gen_iso4217_h.php > backend/iso4217.h" */
#include "iso4217.h"

/* Check for an ISO 4217 currency code */
static int iso4217(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 3)) {
        return 0;
    }

    if (!length_only && data_len) {
        if (!iso4217_numeric(to_int(data + offset, 3))) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Unknown currency code '%.3s'", data + offset);
            return 0;
        }
    }

    return 1;
}

/* Check for percent encoded */
static int pcenc(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    static const char hex_chars[] = "0123456789ABCDEFabcdef";

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de; d++) {
            if (*d == '%') {
                if (de - d < 3) {
                    *p_err_no = 3;
                    *p_err_posn = d - data + 1;
                    strcpy(err_msg, "Invalid % escape");
                    return 0;
                }
                if (strchr(hex_chars, *(++d)) == NULL || strchr(hex_chars, *(++d)) == NULL) {
                    *p_err_no = 3;
                    *p_err_posn = d - data + 1;
                    strcpy(err_msg, "Invalid character for percent encoding");
                    return 0;
                }
            }
        }
    }

    return 1;
}

/* Check for yes/no (1/0) indicator */
static int yesno(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        if (data[offset] != '0' && data[offset] != '1') {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Neither 0 nor 1 for yes or no");
            return 0;
        }
    }

    return 1;
}

/* Check for importer index (GS1 General Specifications 3.8.17) */
static int importeridx(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;

        /* 0-9, A-Z, a-z and "-", "_" */
        if ((*d < '0' && *d != '-') || (*d > '9' && *d < 'A') || (*d > 'Z' && *d < 'a' && *d != '_') || *d > 'z') {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid importer index '%c'", *d);
            return 0;
        }
    }

    return 1;
}

/* Check non-zero */
static int nonzero(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const int val = to_int(data + offset, data_len > max ? max : data_len);

        if (val == 0) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Zero not permitted");
            return 0;
        }
    }

    return 1;
}

/* Check winding direction (0/1/9) (GS1 General Specifications 3.9.1) */
static int winding(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        if (data[offset] != '0' && data[offset] != '1' && data[offset] != '9') {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid winding direction '%c'", data[offset]);
            return 0;
        }
    }

    return 1;
}

/* Check zero */
static int zero(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        if (data[offset] != '0') {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Zero is required");
            return 0;
        }
    }

    return 1;
}

/* Check piece of a trade item (GS1 General Specifications 3.9.6 and 3.9.17) */
static int pieceoftotal(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 4)) {
        return 0;
    }

    if (!length_only && data_len) {
        int pieces, total;

        pieces = to_int(data + offset, 2);
        if (pieces == 0) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Piece number cannot be zero");
            return 0;
        }
        total = to_int(data + offset + 2, 2);
        if (total == 0) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Total number cannot be zero");
            return 0;
        }
        if (pieces > total) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Piece number '%.2s' exceeds total '%.2s'", data + offset, data + offset + 2);
            return 0;
        }
    }

    return 1;
}

/* Check IBAN (ISO 13616) */
static int iban(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }
    /* Do this check separately for backward compatibility */
    if (data_len && data_len < 5) {
        *p_err_no = 4;
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);
        int checksum = 0;
        int given_checksum;

        /* 1st 2 chars alphabetic country code */
        if (!z_isupper(d[0]) || !z_isupper(d[1])) {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            sprintf(err_msg, "Non-alphabetic IBAN country code '%.2s'", d);
            return 0;
        }
        if (!iso3166_alpha2((const char *) d)) {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            sprintf(err_msg, "Invalid IBAN country code '%.2s'", d);
            return 0;
        }
        d += 2;
        /* 2nd 2 chars numeric checksum */
        if (!z_isdigit(d[0]) || !z_isdigit(d[1])) {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            sprintf(err_msg, "Non-numeric IBAN checksum '%.2s'", d);
            return 0;
        }
        given_checksum = to_int(d, 2);
        d += 2;
        for (; d < de; d++) {
            /* 0-9, A-Z */
            if (*d < '0' || (*d > '9' && *d < 'A') || *d > 'Z') {
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                sprintf(err_msg, "Invalid IBAN character '%c'", *d);
                return 0;
            }
            if (*d >= 'A') {
                checksum = checksum * 100 + *d - 'A' + 10;
            } else {
                checksum = checksum * 10 + *d - '0';
            }
            checksum %= 97;
        }

        /* Add in country code */
        checksum = (((checksum * 100) % 97) + (data[offset] - 'A' + 10)) * 100 + data[offset + 1] - 'A' + 10;
        checksum %= 97;

        checksum *= 100; /* Allow for checksum "00" */
        checksum %= 97;

        checksum = 98 - checksum;

        if (checksum != given_checksum) {
            *p_err_no = 3;
            *p_err_posn = offset + 2 + 1;
            sprintf(err_msg, "Bad IBAN checksum '%.2s', expected '%02d'", data + offset + 2, checksum);
            return 0;
        }
    }

    return 1;
}

/* Check CPID does not begin with zero */
static int nozeroprefix(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        /* GS1 General Specifications 3.9.11 "The C/P serial number SHALL NOT begin with a "0" digit, unless the
           entire serial number consists of the single digit '0'." */
        if (data[0] == '0' && data_len != 1) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Zero prefix is not permitted");
            return 0;
        }
    }

    return 1;
}

/* Helper to parse coupon Variable Length Indicator (VLI) and associated field. If `vli_nine` set
 * then a VLI of '9' means no field present */
static const unsigned char *coupon_vli(const unsigned char *data, const int data_len, const unsigned char *d,
            const char *name, const int vli_offset, const int vli_min, const int vli_max, const int vli_nine,
            int *p_err_no, int *p_err_posn, char err_msg[50]) {
    const unsigned char *de;
    int vli;

    if (d - data + 1 > data_len) {
        *p_err_no = 3;
        *p_err_posn = d - data + 1;
        sprintf(err_msg, "%s VLI missing", name);
        return NULL;
    }
    vli = to_int(d, 1);
    if ((vli < vli_min || vli > vli_max) && (vli != 9 || !vli_nine)) {
        *p_err_no = 3;
        *p_err_posn = d - data + 1;
        if (vli < 0) {
            sprintf(err_msg, "Non-numeric %s VLI '%c'", name, *d);
        } else {
            sprintf(err_msg, "Invalid %s VLI '%c'", name, *d);
        }
        return NULL;
    }
    d++;
    if (vli != 9 || !vli_nine) {
        if (d - data + vli + vli_offset > data_len) {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            sprintf(err_msg, "%s incomplete", name);
            return NULL;
        }
        de = d + vli + vli_offset;
        for (; d < de; d++) {
            if (!z_isdigit(*d)) {
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                sprintf(err_msg, "Non-numeric %s '%c'", name, *d);
                return NULL;
            }
        }
    }

    return d;
}

/* Helper to parse coupon value field (numeric) */
static const unsigned char *coupon_val(const unsigned char *data, const int data_len, const unsigned char *d,
            const char *name, const int val_len, int *p_val, int *p_err_no, int *p_err_posn, char err_msg[50]) {
    int val;

    if (d - data + val_len > data_len) {
        *p_err_no = 3;
        *p_err_posn = d - data + 1;
        sprintf(err_msg, "%s incomplete", name);
        return NULL;
    }
    val = to_int(d, val_len);
    if (val < 0) {
        *p_err_no = 3;
        *p_err_posn = d - data + 1;
        sprintf(err_msg, "Non-numeric %s", name);
        return NULL;
    }
    d += val_len;

    if (p_val) {
        *p_val = val;
    }
    return d;
}

/* Check North American Coupon Code */
/* Note all fields including optional must be numeric so type could be N..70 */
static int couponcode(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    /* Minimum possible required fields length = 21
     * (from "North American Coupon Application Guideline Using GS1 DataBar Expanded Symbols R2.0 (Feb 13 2015)")
     * VLI - Variable Length Indicator; GCP - GS1 Company Prefix; OC - Offer Code; SV - Save Value;
     * PPR - Primary Purchase Requirement; PPFC - Primary Purchase Family Code */
    const int min_req_len = 1 /*GCP VLI*/ + 6 /*GCP*/ + 6 /*OC*/ + 1 /*SV VLI*/ + 1 /*SV*/
                            + 1 /*PPR VLI*/ + 1 /*PPR*/ + 1 /*PPR Code*/ + 3 /*PPFC*/;

    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }
    /* Do separately for backward compatibility */
    if (data_len && data_len < min_req_len) {
        *p_err_no = 4;
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        int val;

        data_len += offset;

        /* Required fields */
        d = coupon_vli(data, data_len, d, "Primary GS1 Co. Prefix", 6, 0, 6, 0, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        d = coupon_val(data, data_len, d, "Offer Code", 6, NULL, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        d = coupon_vli(data, data_len, d, "Save Value", 0, 1, 5, 0, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        d = coupon_vli(data, data_len, d, "Primary Purch. Req.", 0, 1, 5, 0, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        d = coupon_val(data, data_len, d, "Primary Purch. Req. Code", 1, &val, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        if (val > 5 && val < 9) {
            *p_err_no = 3;
            *p_err_posn = d - 1 - data + 1;
            sprintf(err_msg, "Invalid Primary Purch. Req. Code '%c'", *(d - 1));
            return 0;
        }
        d = coupon_val(data, data_len, d, "Primary Purch. Family Code", 3, NULL, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }

        /* Optional fields */
        while (d - data < data_len) {
            const int data_field = to_int(d, 1);
            d++;

            if (data_field == 1) {

                d = coupon_val(data, data_len, d, "Add. Purch. Rules Code", 1, &val, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (val > 3) {
                    *p_err_no = 3;
                    *p_err_posn = d - 1 - data + 1;
                    sprintf(err_msg, "Invalid Add. Purch. Rules Code '%c'", *(d - 1));
                    return 0;
                }
                d = coupon_vli(data, data_len, d, "2nd Purch. Req.", 0, 1, 5, 0, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                d = coupon_val(data, data_len, d, "2nd Purch. Req. Code", 1, &val, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (val > 4 && val < 9) {
                    *p_err_no = 3;
                    *p_err_posn = d - 1 - data + 1;
                    sprintf(err_msg, "Invalid 2nd Purch. Req. Code '%c'", *(d - 1));
                    return 0;
                }
                d = coupon_val(data, data_len, d, "2nd Purch. Family Code", 3, NULL, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                d = coupon_vli(data, data_len, d, "2nd Purch. GS1 Co. Prefix", 6, 0, 6, 1, p_err_no, p_err_posn,
                        err_msg);
                if (d == NULL) {
                    return 0;
                }

            } else if (data_field == 2) {

                d = coupon_vli(data, data_len, d, "3rd Purch. Req.", 0, 1, 5, 0, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                d = coupon_val(data, data_len, d, "3rd Purch. Req. Code", 1, &val, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (val > 4 && val < 9) {
                    *p_err_no = 3;
                    *p_err_posn = d - 1 - data + 1;
                    sprintf(err_msg, "Invalid 3rd Purch. Req. Code '%c'", *(d - 1));
                    return 0;
                }
                d = coupon_val(data, data_len, d, "3rd Purch. Family Code", 3, NULL, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                d = coupon_vli(data, data_len, d, "3rd Purch. GS1 Co. Prefix", 6, 0, 6, 1, p_err_no, p_err_posn,
                        err_msg);
                if (d == NULL) {
                    return 0;
                }

            } else if (data_field == 3) {

                d = coupon_val(data, data_len, d, "Expiration Date", 6, NULL, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (!yymmd0(data, data_len, d - 6 - data, 6, 6, p_err_no, p_err_posn, err_msg, 0)) {
                    return 0;
                }

            } else if (data_field == 4) {

                d = coupon_val(data, data_len, d, "Start Date", 6, NULL, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (!yymmd0(data, data_len, d - 6 - data, 6, 6, p_err_no, p_err_posn, err_msg, 0)) {
                    return 0;
                }

            } else if (data_field == 5) {

                d = coupon_vli(data, data_len, d, "Serial Number", 6, 0, 9, 0, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }

            } else if (data_field == 6) {

                d = coupon_vli(data, data_len, d, "Retailer ID", 6, 1, 7, 0, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }

            } else if (data_field == 9) {

                d = coupon_val(data, data_len, d, "Save Value Code", 1, &val, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if ((val > 2 && val < 5) || val > 6) {
                    *p_err_no = 3;
                    *p_err_posn = d - 1 - data + 1;
                    sprintf(err_msg, "Invalid Save Value Code '%c'", *(d - 1));
                    return 0;
                }
                d = coupon_val(data, data_len, d, "Save Value Applies To", 1, &val, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (val > 2) {
                    *p_err_no = 3;
                    *p_err_posn = d - 1 - data + 1;
                    sprintf(err_msg, "Invalid Save Value Applies To '%c'", *(d - 1));
                    return 0;
                }
                d = coupon_val(data, data_len, d, "Store Coupon Flag", 1, NULL, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                d = coupon_val(data, data_len, d, "Don't Multiply Flag", 1, &val, p_err_no, p_err_posn, err_msg);
                if (d == NULL) {
                    return 0;
                }
                if (val > 1) {
                    *p_err_no = 3;
                    *p_err_posn = d - 1 - data + 1;
                    sprintf(err_msg, "Invalid Don't Multiply Flag '%c'", *(d - 1));
                    return 0;
                }

            } else {

                *p_err_no = 3;
                *p_err_posn = d - 1 - data + 1;
                if (data_field < 0) {
                    sprintf(err_msg, "Non-numeric Data Field '%c'", *(d - 1));
                } else {
                    sprintf(err_msg, "Invalid Data Field '%c'", *(d - 1));
                }
                return 0;
            }
        }
    }

    return 1;
}

/* Check North American Positive Offer File */
/* Note max is currently set at 36 numeric digits with remaining 34 characters reserved */
static int couponposoffer(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    /* Minimum possible length = 21
     * (from "GS1 AI (8112) Coupon Data Specifications Release 1.0 (March 2020)")
     * CFMT - Coupon Format; CFID - Coupon Funder ID; VLI - Variable Length Indicator;
     * OC - Offer Code; SN - Serial Number */
    const int min_len = 1 /*CFMT*/ + 1 /*CFID VLI*/ + 6 /*CFID*/ + 6 /*OC*/ + 1 /*SN VLI*/ + 6 /*SN*/;
    const int max_len = 36;

    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }
    /* Do separately for backward compatibility */
    if (data_len && (data_len < min_len || data_len > max_len)) {
        *p_err_no = 4;
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        int val;

        d = coupon_val(data, data_len, d, "Coupon Format", 1, &val, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        if (val != 0 && val != 1) {
            *p_err_no = 3;
            *p_err_posn = d - 1 - data + 1;
            strcpy(err_msg, "Coupon Format must be 0 or 1");
            return 0;
        }
        d = coupon_vli(data, data_len, d, "Coupon Funder ID", 6, 0, 6, 0, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        d = coupon_val(data, data_len, d, "Offer Code", 6, NULL, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        d = coupon_vli(data, data_len, d, "Serial Number", 6, 0, 9, 0, p_err_no, p_err_posn, err_msg);
        if (d == NULL) {
            return 0;
        }
        if (d - data != data_len) {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            strcpy(err_msg, "Reserved trailing characters");
            return 0;
        }
    }

    return 1;
}

/* Check WSG 84 latitude */
static int latitude(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 10)) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);
        uint64_t lat = 0;

        for (; d < de; d++) {
            lat *= 10;
            lat += *d - '0';
        }
        if (lat > 1800000000) {
            *p_err_no = 3;
            *p_err_posn = d - 1 - data + 1;
            strcpy(err_msg, "Invalid latitude");
            return 0;
        }
    }

    return 1;
}

/* Check WSG 84 longitude */
static int longitude(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 10)) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);
        uint64_t lng = 0;

        for (; d < de; d++) {
            lng *= 10;
            lng += *d - '0';
        }
        if (lng > 3600000000) {
            *p_err_no = 3;
            *p_err_posn = d - 1 - data + 1;
            strcpy(err_msg, "Invalid longitude");
            return 0;
        }
    }

    return 1;
}

/* Check AIDC media type (GSCN 22-345 Figure 3.8.22-2) */
static int mediatype(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min || (data_len && data_len < 2)) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);
        unsigned int val = 0;

        for (; d < de; d++) {
            val *= 10;
            val += *d - '0';
        }
        if (val == 0 || (val > 10 && val < 80)) {
            *p_err_no = 3;
            *p_err_posn = d - data + 1;
            strcpy(err_msg, "Invalid AIDC media type");
            return 0;
        }
    }

    return 1;
}

/* Check negative temperature indicator (GSCN 22-353) */
static int hyphen(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de; d++) {
            if (*d != '-') {
                *p_err_no = 3;
                *p_err_posn = d - data + 1;
                strcpy(err_msg, "Invalid temperature indicator (hyphen only)");
                return 0;
            }
        }
    }

    return 1;
}

/* Check for an ISO/IEC 5128 code for the representation of human sexes (GSCN 22-246) */
static int iso5218(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        /* 0 = Not known, 1 = Male, 2 = Female, 9 = Not applicable */
        if (data[offset] != '0' && data[offset] != '1' && data[offset] != '2' && data[offset] != '9') {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Invalid biological sex code (0, 1, 2 or 9 only)");
            return 0;
        }
    }

    return 1;
}

/* Validate sequence indicator, slash-separated (GSCN 22-246) */
static int posinseqslash(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);
        const unsigned char *slash = NULL;
        int pos, tot;

        for (; d < de; d++) {
            if (!z_isdigit(*d)) {
                if (*d != '/') {
                    *p_err_no = 3;
                    *p_err_posn = d - data + 1;
                    sprintf(err_msg, "Invalid character '%c' in sequence", *d);
                    return 0;
                }
                if (slash) {
                    *p_err_no = 3;
                    *p_err_posn = d - data + 1;
                    strcpy(err_msg, "Single sequence separator ('/') only");
                    return 0;
                }
                if (d == data + offset || d + 1 == de) {
                    *p_err_no = 3;
                    *p_err_posn = d - data + 1;
                    strcpy(err_msg, "Sequence separator '/' cannot start or end");
                    return 0;
                }
                slash = d;
            }
        }
        if (!slash) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "No sequence separator ('/')");
            return 0;
        }
        pos = to_int(data + offset, slash - (data + offset));
        if (pos == 0) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Sequence position cannot be zero");
            return 0;
        }
        tot = to_int(slash + 1, de - (slash + 1));
        if (tot == 0) {
            *p_err_no = 3;
            *p_err_posn = slash + 1 - data + 1;
            strcpy(err_msg, "Sequence total cannot be zero");
            return 0;
        }
        if (pos > tot) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "Sequence position greater than total");
            return 0;
        }
    }

    return 1;
}

/* Check that input contains non-digit (GSCN 21-283) */
static int hasnondigit(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {
    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        const unsigned char *d = data + offset;
        const unsigned char *const de = d + (data_len > max ? max : data_len);

        for (; d < de && z_isdigit(*d); d++);

        if (d == de) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            strcpy(err_msg, "A non-digit character is required");
            return 0;
        }
    }

    return 1;
}

/* Check for package type (GSCN 23-272) */
static int packagetype(const unsigned char *data, int data_len, int offset, int min, int max, int *p_err_no,
            int *p_err_posn, char err_msg[50], const int length_only) {

    /* Package type codes https://navigator.gs1.org/edi/codelist-details?name=PackageTypeCode */
    static const char packagetypes2[381][2] = {
        {'1','A'}, {'1','B'}, {'1','D'}, {'1','F'}, {'1','G'}, {'1','W'}, {'2','C'}, {'3','A'}, {'3','H'}, {'4','3'},
        {'4','4'}, {'4','A'}, {'4','B'}, {'4','C'}, {'4','D'}, {'4','F'}, {'4','G'}, {'4','H'}, {'5','H'}, {'5','L'},
        {'5','M'}, {'6','H'}, {'6','P'}, {'7','A'}, {'7','B'}, {'8','A'}, {'8','B'}, {'8','C'}, {'A','A'}, {'A','B'},
        {'A','C'}, {'A','D'}, {'A','F'}, {'A','G'}, {'A','H'}, {'A','I'}, {'A','J'}, {'A','L'}, {'A','M'}, {'A','P'},
        {'A','T'}, {'A','V'}, {'B','4'}, {'B','B'}, {'B','C'}, {'B','D'}, {'B','E'}, {'B','F'}, {'B','G'}, {'B','H'},
        {'B','I'}, {'B','J'}, {'B','K'}, {'B','L'}, {'B','M'}, {'B','N'}, {'B','O'}, {'B','P'}, {'B','Q'}, {'B','R'},
        {'B','S'}, {'B','T'}, {'B','U'}, {'B','V'}, {'B','W'}, {'B','X'}, {'B','Y'}, {'B','Z'}, {'C','A'}, {'C','B'},
        {'C','C'}, {'C','D'}, {'C','E'}, {'C','F'}, {'C','G'}, {'C','H'}, {'C','I'}, {'C','J'}, {'C','K'}, {'C','L'},
        {'C','M'}, {'C','N'}, {'C','O'}, {'C','P'}, {'C','Q'}, {'C','R'}, {'C','S'}, {'C','T'}, {'C','U'}, {'C','V'},
        {'C','W'}, {'C','X'}, {'C','Y'}, {'C','Z'}, {'D','A'}, {'D','B'}, {'D','C'}, {'D','G'}, {'D','H'}, {'D','I'},
        {'D','J'}, {'D','K'}, {'D','L'}, {'D','M'}, {'D','N'}, {'D','P'}, {'D','R'}, {'D','S'}, {'D','T'}, {'D','U'},
        {'D','V'}, {'D','W'}, {'D','X'}, {'D','Y'}, {'E','1'}, {'E','2'}, {'E','3'}, {'E','C'}, {'E','D'}, {'E','E'},
        {'E','F'}, {'E','G'}, {'E','H'}, {'E','I'}, {'E','N'}, {'F','B'}, {'F','C'}, {'F','D'}, {'F','E'}, {'F','I'},
        {'F','L'}, {'F','O'}, {'F','P'}, {'F','R'}, {'F','T'}, {'F','W'}, {'F','X'}, {'G','B'}, {'G','I'}, {'G','L'},
        {'G','R'}, {'G','U'}, {'G','Y'}, {'G','Z'}, {'H','A'}, {'H','B'}, {'H','C'}, {'H','G'}, {'H','N'}, {'H','R'},
        {'I','A'}, {'I','B'}, {'I','C'}, {'I','D'}, {'I','E'}, {'I','F'}, {'I','G'}, {'I','H'}, {'I','K'}, {'I','L'},
        {'I','N'}, {'I','Z'}, {'J','B'}, {'J','C'}, {'J','G'}, {'J','R'}, {'J','T'}, {'J','Y'}, {'K','G'}, {'K','I'},
        {'L','E'}, {'L','G'}, {'L','T'}, {'L','U'}, {'L','V'}, {'L','Z'}, {'M','A'}, {'M','B'}, {'M','C'}, {'M','E'},
        {'M','R'}, {'M','S'}, {'M','T'}, {'M','W'}, {'M','X'}, {'N','A'}, {'N','E'}, {'N','F'}, {'N','G'}, {'N','S'},
        {'N','T'}, {'N','U'}, {'N','V'}, {'O','A'}, {'O','B'}, {'O','C'}, {'O','D'}, {'O','E'}, {'O','F'}, {'O','K'},
        {'O','T'}, {'O','U'}, {'P','2'}, {'P','A'}, {'P','B'}, {'P','C'}, {'P','D'}, {'P','E'}, {'P','F'}, {'P','G'},
        {'P','H'}, {'P','I'}, {'P','J'}, {'P','K'}, {'P','L'}, {'P','N'}, {'P','O'}, {'P','P'}, {'P','R'}, {'P','T'},
        {'P','U'}, {'P','V'}, {'P','X'}, {'P','Y'}, {'P','Z'}, {'Q','A'}, {'Q','B'}, {'Q','C'}, {'Q','D'}, {'Q','F'},
        {'Q','G'}, {'Q','H'}, {'Q','J'}, {'Q','K'}, {'Q','L'}, {'Q','M'}, {'Q','N'}, {'Q','P'}, {'Q','Q'}, {'Q','R'},
        {'Q','S'}, {'R','D'}, {'R','G'}, {'R','J'}, {'R','K'}, {'R','L'}, {'R','O'}, {'R','T'}, {'R','Z'}, {'S','1'},
        {'S','A'}, {'S','B'}, {'S','C'}, {'S','D'}, {'S','E'}, {'S','H'}, {'S','I'}, {'S','K'}, {'S','L'}, {'S','M'},
        {'S','O'}, {'S','P'}, {'S','S'}, {'S','T'}, {'S','U'}, {'S','V'}, {'S','W'}, {'S','X'}, {'S','Y'}, {'S','Z'},
        {'T','1'}, {'T','B'}, {'T','C'}, {'T','D'}, {'T','E'}, {'T','G'}, {'T','I'}, {'T','K'}, {'T','L'}, {'T','N'},
        {'T','O'}, {'T','R'}, {'T','S'}, {'T','T'}, {'T','U'}, {'T','V'}, {'T','W'}, {'T','Y'}, {'T','Z'}, {'U','C'},
        {'U','N'}, {'V','A'}, {'V','G'}, {'V','I'}, {'V','K'}, {'V','L'}, {'V','N'}, {'V','O'}, {'V','P'}, {'V','Q'},
        {'V','R'}, {'V','S'}, {'V','Y'}, {'W','A'}, {'W','B'}, {'W','C'}, {'W','D'}, {'W','F'}, {'W','G'}, {'W','H'},
        {'W','J'}, {'W','K'}, {'W','L'}, {'W','M'}, {'W','N'}, {'W','P'}, {'W','Q'}, {'W','R'}, {'W','S'}, {'W','T'},
        {'W','U'}, {'W','V'}, {'W','W'}, {'W','X'}, {'W','Y'}, {'W','Z'}, {'X','3'}, {'X','A'}, {'X','B'}, {'X','C'},
        {'X','D'}, {'X','F'}, {'X','G'}, {'X','H'}, {'X','J'}, {'X','K'}, {'Y','A'}, {'Y','B'}, {'Y','C'}, {'Y','D'},
        {'Y','F'}, {'Y','G'}, {'Y','H'}, {'Y','J'}, {'Y','K'}, {'Y','L'}, {'Y','M'}, {'Y','N'}, {'Y','P'}, {'Y','Q'},
        {'Y','R'}, {'Y','S'}, {'Y','T'}, {'Y','V'}, {'Y','W'}, {'Y','X'}, {'Y','Y'}, {'Y','Z'}, {'Z','A'}, {'Z','B'},
        {'Z','C'}, {'Z','D'}, {'Z','F'}, {'Z','G'}, {'Z','H'}, {'Z','J'}, {'Z','K'}, {'Z','L'}, {'Z','M'}, {'Z','N'},
        {'Z','P'}, {'Z','Q'}, {'Z','R'}, {'Z','S'}, {'Z','T'}, {'Z','U'}, {'Z','V'}, {'Z','W'}, {'Z','X'}, {'Z','Y'},
        {'Z','Z'},
    };
    static const char packagetypes3[48][3] = {
        {'2','0','0'}, {'2','0','1'}, {'2','0','2'}, {'2','0','3'}, {'2','0','4'},
        {'2','0','5'}, {'2','0','6'}, {'2','1','0'}, {'2','1','1'}, {'2','1','2'},
        {'A','P','E'}, {'B','G','E'}, {'B','M','E'}, {'B','R','I'}, {'C','B','L'},
        {'C','C','E'}, {'D','P','E'}, {'F','O','B'}, {'F','P','E'}, {'L','A','B'},
        {'M','P','E'}, {'O','P','E'}, {'P','A','E'}, {'P','L','P'}, {'P','O','P'},
        {'P','P','E'}, {'P','U','E'}, {'R','B','1'}, {'R','B','2'}, {'R','B','3'},
        {'R','C','B'}, {'S','E','C'}, {'S','T','L'}, {'T','E','V'}, {'T','H','E'},
        {'T','R','E'}, {'T','T','E'}, {'T','W','E'}, {'U','U','E'}, {'W','R','P'},
        {'X','1','1'}, {'X','1','2'}, {'X','1','5'}, {'X','1','6'}, {'X','1','7'},
        {'X','1','8'}, {'X','1','9'}, {'X','2','0'},
    };

    (void)max;

    data_len = data_len < offset ? 0 : data_len - offset;

    if (data_len < min) {
        return 0;
    }

    if (!length_only && data_len) {
        /* Adapted from GS1 Syntax Dictionary and Linters
           https://github.com/gs1/gs1-syntax-dictionary/blob/main/src/lint_packagetype.c */
        /* SPDX-License-Identifier: Apache-2.0 */
        const char *const d = (const char *const) (data + offset);
        int valid = 0;

        assert(2 /*single 8/9*/ + ARRAY_SIZE(packagetypes2) + ARRAY_SIZE(packagetypes3) == 431);

        if (data_len == 1) {
            valid = *d == '8' || *d == '9';
        } else if (data_len == 2) {
            int s = 0;
            int e = ARRAY_SIZE(packagetypes2);

            while (s < e) {
                const int m = s + (e - s) / 2;
                const int cmp = memcmp(packagetypes2[m], d, 2);
                if (cmp < 0) {
                    s = m + 1;
                } else if (cmp > 0) {
                    e = m;
                } else {
                    valid = 1;
                    break;
                }
            }
        } else if (data_len == 3) {
            int s = 0;
            int e = ARRAY_SIZE(packagetypes3);

            while (s < e) {
                const int m = s + (e - s) / 2;
                const int cmp = memcmp(packagetypes3[m], d, 3);
                if (cmp < 0) {
                    s = m + 1;
                } else if (cmp > 0) {
                    e = m;
                } else {
                    valid = 1;
                    break;
                }
            }
        }

        if (!valid) {
            *p_err_no = 3;
            *p_err_posn = offset + 1;
            sprintf(err_msg, "Invalid package type '%.*s'", data_len, d);
            return 0;
        }
    }

    return 1;
}

/* Generated by "php backend/tools/gen_gs1_linter.php > backend/gs1_lint.h" */
#include "gs1_lint.h"

/* Verify a GS1 input string */
INTERNAL int gs1_verify(struct zint_symbol *symbol, const unsigned char source[], const int length,
                unsigned char reduced[]) {
    int i, j;
    int error_value = 0;
    int bracket_level = 0, max_bracket_level = 0;
    int ai_length, ai_latch;
    int max_ai_length = 0, min_ai_length = 5;
    int max_ai_pos = 0, min_ai_pos = 0; /* Suppress gcc 14 "-Wmaybe-uninitialized" false positives */
    int ai_zero_len_no_data = 0, ai_single_digit = 0, ai_nonnumeric = 0;
    int ai_nonnumeric_pos = 0; /* Suppress gcc 14 "-Wmaybe-uninitialized" false positive */
    const char obracket = symbol->input_mode & GS1PARENS_MODE ? '(' : '[';
    const char cbracket = symbol->input_mode & GS1PARENS_MODE ? ')' : ']';
    const int ai_max = chr_cnt(source, length, obracket) + 1; /* Plus 1 so non-zero */
    int *ai_value = (int *) z_alloca(sizeof(int) * ai_max);
    int *ai_location = (int *) z_alloca(sizeof(int) * ai_max);
    int *data_location = (int *) z_alloca(sizeof(int) * ai_max);
    int *data_length = (int *) z_alloca(sizeof(int) * ai_max);

    /* Detect control and extended ASCII characters */
    for (i = 0; i < length; i++) {
        if (source[i] >= 128) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 250, "Extended ASCII characters are not supported by GS1");
        }
        if (source[i] == '\0') {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 262, "NUL characters not permitted in GS1 mode");
        }
        if (source[i] < 32) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 251, "Control characters are not supported by GS1");
        }
        if (source[i] == 127) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 263, "DEL characters are not supported by GS1");
        }
    }

    if (source[0] != obracket) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 252, "Data does not start with an AI");
    }

    /* Check the balance of the brackets & AI lengths */
    ai_length = 0;
    ai_latch = 0;
    for (i = 0; i < length; i++) {
        if (source[i] == obracket) {
            bracket_level++;
            if (bracket_level > max_bracket_level) {
                max_bracket_level = bracket_level;
            }
            ai_latch = 1;
        } else if (source[i] == cbracket && bracket_level) {
            bracket_level--;
            if (ai_length > max_ai_length) {
                max_ai_length = ai_length;
                max_ai_pos = i - ai_length;
            }
            if (ai_length < min_ai_length) {
                min_ai_length = ai_length;
                min_ai_pos = i - ai_length;
            }
            /* Check zero-length AI has data */
            if (ai_length == 0 && (i + 1 == length || source[i + 1] == obracket)) {
                ai_zero_len_no_data = 1;
            } else if (ai_length == 1) {
                ai_single_digit = 1;
            }
            ai_length = 0;
            ai_latch = 0;
        } else if (ai_latch) {
            ai_length++;
            if (!z_isdigit(source[i])) {
                ai_nonnumeric = 1;
                ai_nonnumeric_pos = i - ai_length + 1;
            }
        }
    }

    if (bracket_level != 0) {
        /* Not all brackets are closed */
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 253, "Malformed AI in input (brackets don\'t match)");
    }

    if (max_bracket_level > 1) {
        /* Nested brackets */
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 254, "Found nested brackets in input");
    }

    if (max_ai_length > 4) {
        /* AI is too long */
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 255, "Invalid AI at position %d in input (AI too long)",
                        max_ai_pos);
    }

    if (min_ai_length <= 1) {
        /* Allow too short AI if GS1NOCHECK_MODE and no single-digit AIs and all zero-length AIs have some data
           - permits dummy "[]" workaround for ticket #204 data with no valid AI */
        if (!(symbol->input_mode & GS1NOCHECK_MODE) || ai_single_digit || ai_zero_len_no_data) {
            /* AI is too short */
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 256, "Invalid AI at position %d in input (AI too short)",
                            min_ai_pos);
        }
    }

    if (ai_nonnumeric) {
        /* Non-numeric data in AI */
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 257,
                        "Invalid AI at position %d in input (non-numeric characters in AI)", ai_nonnumeric_pos);
    }

    if (!(symbol->input_mode & GS1NOCHECK_MODE)) {
        int ai_count = 0;
        for (i = 1; i < length; i++) {
            if (source[i - 1] == obracket) {
                ai_location[ai_count] = i;
                for (j = 1; source[i + j] != cbracket; j++);
                ai_value[ai_count] = to_int(source + i, j);
                ai_count++;
                i += j;
            }
        }

        for (i = 0; i < ai_count; i++) {
            if (ai_value[i] >= 1000) {
                data_location[i] = ai_location[i] + 5;
            } else if (ai_value[i] >= 100) {
                data_location[i] = ai_location[i] + 4;
            } else {
                data_location[i] = ai_location[i] + 3;
            }
            data_length[i] = 0;
            while ((data_location[i] + data_length[i] < length)
                        && (source[data_location[i] + data_length[i]] != obracket)) {
                data_length[i]++;
            }
            if (data_length[i] == 0) {
                /* No data for given AI */
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 258, "Empty data field in input");
            }
        }

        /* Check for valid AI values and data lengths according to GS1 General
           Specifications Release 21.0.1, January 2021 */
        for (i = 0; i < ai_count; i++) {
            int err_no, err_posn;
            char err_msg[50];
            if (!gs1_lint(ai_value[i], source + data_location[i], data_length[i], &err_no, &err_posn, err_msg)) {
                if (err_no == 1) {
                    errtxtf(0, symbol, 260, "Invalid AI (%02d)", ai_value[i]);
                } else if (err_no == 2 || err_no == 4) { /* 4 is backward-incompatible bad length */
                    errtxtf(0, symbol, 259, "Invalid data length for AI (%02d)", ai_value[i]);
                } else {
                    errtxtf(0, symbol, 261, "AI (%1$02d) position %2$d: %3$s", ai_value[i], err_posn, err_msg);
                }
                /* For backward compatibility only error on unknown AI or bad length */
                if (err_no == 1 || err_no == 2) {
                    return ZINT_ERROR_INVALID_DATA;
                }
                error_value = ZINT_WARN_NONCOMPLIANT;
            }
        }
    }

    /* Resolve AI data - put resulting string in 'reduced' */
    j = 0;
    ai_latch = 1;
    for (i = 0; i < length; i++) {
        if (source[i] == obracket) {
            bracket_level++;
            /* Start of an AI string */
            if (ai_latch == 0) {
                reduced[j++] = '\x1D';
            }
            if (i + 1 != length) {
                int last_ai = to_int(source + i + 1, 2);
                ai_latch = 0;
                /* The following values from "GS1 General Specifications Release 21.0.1"
                   Figure 7.8.4-2 "Element strings with predefined length using GS1 Application Identifiers" */
                if (
                        ((last_ai >= 0) && (last_ai <= 4))
                        || ((last_ai >= 11) && (last_ai <= 20))
                        /* NOTE: as noted by Terry Burton the following complies with ISO/IEC 24724:2011 Table D.1,
                           but clashes with TPX AI [235], introduced May 2019; awaiting feedback from GS1 */
                        || (last_ai == 23) /* legacy support */ /* TODO: probably remove */
                        || ((last_ai >= 31) && (last_ai <= 36))
                        || (last_ai == 41)
                        ) {
                    ai_latch = 1;
                }
            }
        } else if (source[i] == cbracket && bracket_level) {
            /* The closing bracket is simply dropped from the input */
            bracket_level--;
        } else {
            reduced[j++] = source[i];
        }
    }
    reduced[j] = '\0';

    /* The character '\x1D' (GS) in the reduced string refers to the FNC1 character */
    return error_value;
}

/* Helper to return standard GS1 check digit (GS1 General Specifications 7.9.1) */
INTERNAL char gs1_check_digit(const unsigned char source[], const int length) {
    int i;
    int count = 0;
    int factor = length & 1 ? 3 : 1;

    for (i = 0; i < length; i++) {
        count += factor * ctoi(source[i]);
        factor ^= 0x02; /* Toggles 1 and 3 */
    }

    return itoc((10 - (count % 10)) % 10);
}

/* Helper to expose `iso3166_alpha2()` */
INTERNAL int gs1_iso3166_alpha2(const unsigned char *cc) {
    return iso3166_alpha2((const char *) cc);
}

/* vim: set ts=4 sw=4 et : */
