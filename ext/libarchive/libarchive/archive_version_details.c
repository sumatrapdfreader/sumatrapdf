/*-
 * Copyright (c) 2009-2012,2014 Michihiro NAKAJIMA
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif
#ifdef HAVE_LZ4_H
#include <lz4.h>
#endif
#ifdef HAVE_ZSTD_H
#include <zstd.h>
#include <stdio.h>
#endif
#ifdef HAVE_LZO_LZOCONF_H
#include <lzo/lzoconf.h>
#endif
#if HAVE_LIBXML_XMLVERSION_H
#include <libxml/xmlversion.h>
#elif HAVE_BSDXML_H
#include <bsdxml.h>
#elif HAVE_EXPAT_H
#include <expat.h>
#endif
#if HAVE_MBEDTLS_VERSION_H
#include <mbedtls/version.h>
#endif
#if HAVE_NETTLE_VERSION_H
#include <nettle/version.h>
#include <stdio.h>
#endif
#if HAVE_OPENSSL_OPENSSLV_H
#include <openssl/opensslv.h>
#include <stdio.h>
#endif
#if HAVE_ICONV_H
#include <iconv.h>
#endif
#if HAVE_PCRE_H
#include <pcre.h>
#endif
#if HAVE_PCRE2_H
#include <pcre2.h>
#endif

#include "archive.h"
#include "archive_private.h"
#include "archive_string.h"
#include "archive_cryptor_private.h"
#include "archive_digest_private.h"

static void
archive_regex_version(struct archive_string* str)
{
#if HAVE_LIBPCREPOSIX && HAVE_PCRE_H
	archive_strcat(str, " libpcre/");
	archive_strcat(str, archive_libpcre_version());
#elif HAVE_LIBPCRE2POSIX && HAVE_PCRE2_H
	archive_strcat(str, " libpcre2/");
	archive_strcat(str, archive_libpcre2_version());
#else
	(void)str; /* UNUSED */
#endif
}

static void
archive_xml_version(struct archive_string* str)
{
#if HAVE_LIBXML_XMLVERSION_H && HAVE_LIBXML2
	archive_strcat(str, " libxml2/");
	archive_strcat(str, archive_libxml2_version());
#elif HAVE_BSDXML_H && HAVE_LIBBSDXML
	archive_strcat(str, " bsdxml/");
	archive_strcat(str, archive_libbsdxml_version());
#elif HAVE_EXPAT_H && HAVE_LIBEXPAT
	archive_strcat(str, " expat/");
	archive_strcat(str, archive_libexpat_version());
#else
	(void)str; /* UNUSED */
#endif
}

static void
archive_libb2_version(struct archive_string* str)
{
	archive_strcat(str, " libb2/");
#if HAVE_BLAKE2_H && HAVE_LIBB2
#if defined(LIBB2_PKGCONFIG_VERSION)
	archive_strcat(str, LIBB2_PKGCONFIG_VERSION);
#else
	archive_strcat(str, "system");
#endif
#else
	archive_strcat(str, "bundled");
#endif
}

static void
archive_crypto_version(struct archive_string* str)
{
#if defined(ARCHIVE_CRYPTOR_USE_Apple_CommonCrypto)
	archive_strcat(str, " CommonCrypto/");
	archive_strcat(str, archive_commoncrypto_version());
#endif
#if defined(ARCHIVE_CRYPTOR_USE_CNG)
	archive_strcat(str, " cng/");
	archive_strcat(str, archive_cng_version());
#endif
#if defined(ARCHIVE_CRYPTOR_USE_MBED)
	archive_strcat(str, " mbedtls/");
	archive_strcat(str, archive_mbedtls_version());
#endif
#if defined(ARCHIVE_CRYPTOR_USE_NETTLE)
	archive_strcat(str, " nettle/");
	archive_strcat(str, archive_nettle_version());
#endif
#if defined(ARCHIVE_CRYPTOR_USE_OPENSSL)
	archive_strcat(str, " openssl/");
	archive_strcat(str, archive_openssl_version());
#endif
#if defined(ARCHIVE_CRYPTOR_USE_LIBMD)
	archive_strcat(str, " libmd/");
	archive_strcat(str, archive_libmd_version());
#endif
	// Just in case
	(void)str; /* UNUSED */
}

const char *
archive_version_details(void)
{
	static struct archive_string str;
	static int init = 0;
	const char *zlib = archive_zlib_version();
	const char *liblzma = archive_liblzma_version();
	const char *bzlib = archive_bzlib_version();
	const char *liblz4 = archive_liblz4_version();
	const char *libzstd = archive_libzstd_version();
	const char *liblzo = archive_liblzo2_version();
	const char *libiconv = archive_libiconv_version();
	const char *libacl = archive_libacl_version();
	const char *librichacl = archive_librichacl_version();
	const char *libattr = archive_libattr_version();

	if (!init) {
		archive_string_init(&str);

		archive_strcat(&str, ARCHIVE_VERSION_STRING);
		if (zlib) {
			archive_strcat(&str, " zlib/");
			archive_strcat(&str, zlib);
		}
		if (liblzma) {
			archive_strcat(&str, " liblzma/");
			archive_strcat(&str, liblzma);
		}
		if (bzlib) {
			const char *p = bzlib;
			const char *sep = strchr(p, ',');
			if (sep == NULL)
				sep = p + strlen(p);
			archive_strcat(&str, " bz2lib/");
			archive_strncat(&str, p, sep - p);
		}
		if (liblz4) {
			archive_strcat(&str, " liblz4/");
			archive_strcat(&str, liblz4);
		}
		if (libzstd) {
			archive_strcat(&str, " libzstd/");
			archive_strcat(&str, libzstd);
		}
		if (liblzo) {
			archive_strcat(&str, " liblzo2/");
			archive_strcat(&str, liblzo);
		}
		archive_xml_version(&str);
		archive_regex_version(&str);
		archive_crypto_version(&str);
		archive_libb2_version(&str);
		if (librichacl) {
			archive_strcat(&str, " librichacl/");
			archive_strcat(&str, librichacl);
		}
		if (libacl) {
			archive_strcat(&str, " libacl/");
			archive_strcat(&str, libacl);
		}
		if (libattr) {
			archive_strcat(&str, " libattr/");
			archive_strcat(&str, libattr);
		}
		if (libiconv) {
			archive_strcat(&str, " libiconv/");
			archive_strcat(&str, libiconv);
		}
		init = 1;
	}
	return str.s;
}

const char *
archive_zlib_version(void)
{
#if HAVE_ZLIB_H && HAVE_LIBZ
	return zlibVersion();
#else
	return NULL;
#endif
}

const char *
archive_liblzma_version(void)
{
#if HAVE_LZMA_H && HAVE_LIBLZMA
	return lzma_version_string();
#else
	return NULL;
#endif
}

const char *
archive_bzlib_version(void)
{
#if HAVE_BZLIB_H && HAVE_LIBBZ2
	return BZ2_bzlibVersion();
#else
	return NULL;
#endif
}

const char *
archive_liblz4_version(void)
{
#if HAVE_LZ4_H && HAVE_LIBLZ4
#if LZ4_VERSION_NUMBER > 10705
	return LZ4_versionString();
#elif LZ4_VERSION_NUMBER > 10300
	div_t major = div(LZ4_versionNumber(), 10000);
	div_t minor = div(major.rem, 100);
	static char lz4_version[9];
	snprintf(lz4_version, 9, "%d.%d.%d", major.quot, minor.quot, minor.rem);
	return lz4_version;
#else
#define str(s) #s
#define NUMBER(x) str(x)
	return NUMBER(LZ4_VERSION_MAJOR) "." NUMBER(LZ4_VERSION_MINOR) "." NUMBER(LZ4_VERSION_RELEASE);
#undef NUMBER
#undef str
#endif
#else
	return NULL;
#endif
}

const char *
archive_libzstd_version(void)
{
#if HAVE_ZSTD_H && HAVE_LIBZSTD
#if ZSTD_VERSION_NUMBER > 10300
	return ZSTD_versionString();
#else
	div_t major = div(ZSTD_versionNumber(), 10000);
	div_t minor = div(major.rem, 100);
	static char zstd_version[9];
	snprintf(zstd_version, 9, "%d.%d.%d", major.quot, minor.quot, minor.rem);
	return zstd_version;
#endif
#else
	return NULL;
#endif
}

const char *
archive_liblzo2_version(void)
{
#if HAVE_LZO_LZOCONF_H && HAVE_LIBLZO2
	return LZO_VERSION_STRING;
#else
	return NULL;
#endif
}

const char *
archive_libbsdxml_version(void)
{
#if HAVE_BSDXML_H && HAVE_LIBBSDXML
	return XML_ExpatVersion();
#else
	return NULL;
#endif
}

const char *
archive_libxml2_version(void)
{
#if HAVE_LIBXML_XMLVERSION_H && HAVE_LIBXML2
	return LIBXML_DOTTED_VERSION;
#else
	return NULL;
#endif
}

const char *
archive_libexpat_version(void)
{
#if HAVE_EXPAT_H && HAVE_LIBEXPAT
	return XML_ExpatVersion();
#else
	return NULL;
#endif
}

const char *
archive_mbedtls_version(void)
{
#if defined(ARCHIVE_CRYPTOR_USE_MBED) || defined(ARCHIVE_CRYPTO_MBED)
	static char mbed_version[9];
	mbedtls_version_get_string(mbed_version);
	return mbed_version;
#else
	return NULL;
#endif
}

const char *
archive_nettle_version(void)
{
#if defined(ARCHIVE_CRYPTOR_USE_NETTLE) || defined(ARCHIVE_CRYPTO_NETTLE)
	static char nettle_version[6];
	snprintf(nettle_version, 6, "%d.%d", nettle_version_major(), nettle_version_minor());
	return nettle_version;
#else
	return NULL;
#endif
}

const char *
archive_openssl_version(void)
{
#if defined(ARCHIVE_CRYPTOR_USE_OPENSSL) || defined(ARCHIVE_CRYPTO_OPENSSL)
#ifdef OPENSSL_VERSION_STR
	return OPENSSL_VERSION_STR;
#else
#define OPENSSL_MAJOR (OPENSSL_VERSION_NUMBER >> 28)
#define OPENSSL_MINOR ((OPENSSL_VERSION_NUMBER >> 20) & 0xFF)
	static char openssl_version[6];
	snprintf(openssl_version, 6, "%ld.%ld", OPENSSL_MAJOR, OPENSSL_MINOR);
	return openssl_version;
#undef OPENSSL_MAJOR
#undef OPENSSL_MINOR
#endif
#else
	return NULL;
#endif
}

const char *
archive_libmd_version(void)
{
#if defined(ARCHIVE_CRYPTOR_USE_LIBMD) || defined(ARCHIVE_CRYPTO_LIBMD)
	return "system";
#else
	return NULL;
#endif
}

const char *
archive_commoncrypto_version(void)
{
#if defined(ARCHIVE_CRYPTOR_USE_Apple_CommonCrypto) || defined(ARCHIVE_CRYPTO_CommonCrypto)
	return "system";
#else
	return NULL;
#endif
}

const char *
archive_cng_version(void)
{
#if defined(ARCHIVE_CRYPTOR_USE_CNG) || defined(ARCHIVE_CRYPTO_CNG)
#ifdef BCRYPT_HASH_INTERFACE_MAJORVERSION_2
	return "2.0";
#else
	return "1.0";
#endif
#else
	return NULL;
#endif
}

const char *
archive_wincrypt_version(void)
{
	return NULL;
}

const char *
archive_librichacl_version(void)
{
#if HAVE_LIBRICHACL
#if defined(LIBRICHACL_PKGCONFIG_VERSION)
	return LIBRICHACL_PKGCONFIG_VERSION;
#else
	return "system";
#endif
#else
	return NULL;
#endif
}

const char *
archive_libacl_version(void)
{
#if HAVE_LIBACL
#if defined(LIBACL_PKGCONFIG_VERSION)
	return LIBACL_PKGCONFIG_VERSION;
#else
	return "system";
#endif
#else
	return NULL;
#endif
}

const char *
archive_libattr_version(void)
{
#if HAVE_LIBATTR
#if defined(LIBATTR_PKGCONFIG_VERSION)
	return LIBATTR_PKGCONFIG_VERSION;
#else
	return "system";
#endif
#else
	return NULL;
#endif
}

const char *
archive_libiconv_version(void)
{
#if HAVE_LIBCHARSET && HAVE_ICONV_H
	char major = _libiconv_version >> 8;
	char minor = _libiconv_version & 0xFF;
	static char charset_version[6];
	snprintf(charset_version, 6, "%hhd.%hhd", major, minor);
	return charset_version;
#else
	return NULL;
#endif
}

const char *
archive_libpcre_version(void)
{
#if HAVE_LIBPCREPOSIX && HAVE_PCRE_H
#define str(s) #s
#define NUMBER(x) str(x)
	return NUMBER(PCRE_MAJOR) "." NUMBER(PCRE_MINOR);
#undef NUMBER
#undef str
#else
	return NULL;
#endif
}

const char *
archive_libpcre2_version(void)
{
#if HAVE_LIBPCRE2POSIX && HAVE_PCRE2_H
#define str(s) #s
#define NUMBER(x) str(x)
	return NUMBER(PCRE2_MAJOR) "." NUMBER(PCRE2_MINOR);
#undef NUMBER
#undef str
#else
	return NULL;
#endif
}
