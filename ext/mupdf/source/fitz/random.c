// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h> // for GetSystemTime and CryptGenRandom
#else
#include <sys/time.h> // for gettimeofday()
#include <stdlib.h> // for srandom(), random(), arc4random_buf()
#ifdef __APPLE__
#include <sys/random.h> // for getentropy()
#else
#include <unistd.h> // for getentropy()
#endif
#endif

#ifdef _WIN32

static void getentropy_fallback(unsigned char *s, int n)
{
	int i;
	SYSTEMTIME system_time;

	GetSystemTime(&system_time);
	srand(system_time.wSecond ^ system_time.wMilliseconds);
	for (i = 0; i < n; i += 4)
		fz_pack_uint32_le(s + i, rand());
}

static int getentropy_win32(unsigned char *entropy, size_t len)
{
	HCRYPTPROV prov;
	BOOL ok;
	if (!CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		return 1;
	ok = CryptGenRandom(prov, (DWORD)len, entropy);
	CryptReleaseContext(prov, 0);
	return !ok;
}

#else

static void getentropy_fallback(unsigned char *s, int n)
{
	struct timeval now;
	int i;
	gettimeofday(&now, NULL);
	srandom(now.tv_sec ^ now.tv_usec);
	for (i = 0; i < n; i += 4)
		fz_pack_uint32_le(s + i, random());
}

#endif

void fz_memrnd(fz_context *ctx, unsigned char *data, int len)
{
#ifdef CLUSTER
	memset(data, 0x55, len);
#else
	// (Re-)initialize chacha20 stream cipher on first invocation,
	// or when the counter wraps around.
	if (ctx->seed.s[12] == 0)
	{
		unsigned char entropy[44];

#if defined(_WIN32)
		if (getentropy_win32(entropy, sizeof entropy))
			getentropy_fallback(entropy, sizeof entropy);
#elif defined(EMSCRIPTEN)
		// __wasi_random_get is not yet widely supported, so we can't call getentropy
		getentropy_fallback(entropy, sizeof entropy);
#elif defined(__ANDROID__) && (__ANDROID_API__ < 28)
		// Android before 9.0 does not have getentropy(), but does have
		// arc4random_buf that sources from /dev/urandom.
		arc4random_buf(entropy, sizeof entropy);
#else
		if (getentropy(entropy, sizeof entropy) < 0)
			getentropy_fallback(entropy, sizeof entropy);
#endif /* EMSCRIPTEN */

		fz_chacha20_init(&ctx->seed, entropy, entropy + 32, 0);
	}

	memset(data, 0, len);
	fz_chacha20_encrypt(&ctx->seed, data, data, len);
#endif /* CLUSTER */
}
