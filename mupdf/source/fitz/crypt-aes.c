/*
 *	FIPS-197 compliant AES implementation
 *
 *	Copyright (C) 2006-2007 Christophe Devine
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *
 *	* Redistributions of source code _must_ retain the above copyright
 *		notice, this list of conditions and the following disclaimer.
 *	* Redistributions in binary form may or may not reproduce the above
 *		copyright notice, this list of conditions and the following
 *		disclaimer in the documentation and/or other materials provided
 *		with the distribution.
 *	* Neither the name of XySSL nor the names of its contributors may be
 *		used to endorse or promote products derived from this software
 *		without specific prior written permission.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *	OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *	TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *	The AES block cipher was designed by Vincent Rijmen and Joan Daemen.
 *
 *	http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
 *	http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf
 */

#include "mupdf/fitz.h"

#define aes_context fz_aes

/* AES block cipher implementation from XYSSL */

/*
 * 32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)					\
{								\
	(n) = ( (unsigned long) (b)[(i)] )			\
		| ( (unsigned long) (b)[(i) + 1] << 8 )		\
		| ( (unsigned long) (b)[(i) + 2] << 16 )	\
		| ( (unsigned long) (b)[(i) + 3] << 24 );	\
}
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)				\
{							\
	(b)[(i) ] = (unsigned char) ( (n) );		\
	(b)[(i) + 1] = (unsigned char) ( (n) >> 8 );	\
	(b)[(i) + 2] = (unsigned char) ( (n) >> 16 );	\
	(b)[(i) + 3] = (unsigned char) ( (n) >> 24 );	\
}
#endif

/*
 * Forward S-box & tables
 */
static unsigned char FSb[256];
static unsigned long FT0[256];
static unsigned long FT1[256];
static unsigned long FT2[256];
static unsigned long FT3[256];

/*
 * Reverse S-box & tables
 */
static unsigned char RSb[256];
static unsigned long RT0[256];
static unsigned long RT1[256];
static unsigned long RT2[256];
static unsigned long RT3[256];

/*
 * Round constants
 */
static unsigned long RCON[10];

/*
 * Tables generation code
 */
#define ROTL8(x) ( ( x << 8 ) & 0xFFFFFFFF ) | ( x >> 24 )
#define XTIME(x) ( ( x << 1 ) ^ ( ( x & 0x80 ) ? 0x1B : 0x00 ) )
#define MUL(x,y) ( ( x && y ) ? pow[(log[x]+log[y]) % 255] : 0 )

static int aes_init_done = 0;

static void aes_gen_tables( void )
{
	int i, x, y, z;
	int pow[256];
	int log[256];

	/*
	 * compute pow and log tables over GF(2^8)
	 */
	for( i = 0, x = 1; i < 256; i++ )
	{
		pow[i] = x;
		log[x] = i;
		x = ( x ^ XTIME( x ) ) & 0xFF;
	}

	/*
	 * calculate the round constants
	 */
	for( i = 0, x = 1; i < 10; i++ )
	{
		RCON[i] = (unsigned long) x;
		x = XTIME( x ) & 0xFF;
	}

	/*
	 * generate the forward and reverse S-boxes
	 */
	FSb[0x00] = 0x63;
	RSb[0x63] = 0x00;

	for( i = 1; i < 256; i++ )
	{
		x = pow[255 - log[i]];

		y = x; y = ( (y << 1) | (y >> 7) ) & 0xFF;
		x ^= y; y = ( (y << 1) | (y >> 7) ) & 0xFF;
		x ^= y; y = ( (y << 1) | (y >> 7) ) & 0xFF;
		x ^= y; y = ( (y << 1) | (y >> 7) ) & 0xFF;
		x ^= y ^ 0x63;

		FSb[i] = (unsigned char) x;
		RSb[x] = (unsigned char) i;
	}

	/*
	 * generate the forward and reverse tables
	 */
	for( i = 0; i < 256; i++ )
	{
		x = FSb[i];
		y = XTIME( x ) & 0xFF;
		z = ( y ^ x ) & 0xFF;

		FT0[i] = ( (unsigned long) y ) ^
		( (unsigned long) x <<	8 ) ^
		( (unsigned long) x << 16 ) ^
		( (unsigned long) z << 24 );

		FT1[i] = ROTL8( FT0[i] );
		FT2[i] = ROTL8( FT1[i] );
		FT3[i] = ROTL8( FT2[i] );

		x = RSb[i];

		RT0[i] = ( (unsigned long) MUL( 0x0E, x ) ) ^
		( (unsigned long) MUL( 0x09, x ) << 8 ) ^
		( (unsigned long) MUL( 0x0D, x ) << 16 ) ^
		( (unsigned long) MUL( 0x0B, x ) << 24 );

		RT1[i] = ROTL8( RT0[i] );
		RT2[i] = ROTL8( RT1[i] );
		RT3[i] = ROTL8( RT2[i] );
	}
}

/*
 * AES key schedule (encryption)
 */
int aes_setkey_enc( aes_context *ctx, const unsigned char *key, int keysize )
{
	int i;
	unsigned long *RK;

#if !defined(XYSSL_AES_ROM_TABLES)
	if( aes_init_done == 0 )
	{
		aes_gen_tables();
		aes_init_done = 1;
	}
#endif

	switch( keysize )
	{
	case 128: ctx->nr = 10; break;
	case 192: ctx->nr = 12; break;
	case 256: ctx->nr = 14; break;
	default : return 1;
	}

#if defined(PADLOCK_ALIGN16)
	ctx->rk = RK = PADLOCK_ALIGN16( ctx->buf );
#else
	ctx->rk = RK = ctx->buf;
#endif

	for( i = 0; i < (keysize >> 5); i++ )
	{
		GET_ULONG_LE( RK[i], key, i << 2 );
	}

	switch( ctx->nr )
	{
	case 10:

		for( i = 0; i < 10; i++, RK += 4 )
		{
			RK[4] = RK[0] ^ RCON[i] ^
				( FSb[ ( RK[3] >> 8 ) & 0xFF ] ) ^
				( FSb[ ( RK[3] >> 16 ) & 0xFF ] << 8 ) ^
				( FSb[ ( RK[3] >> 24 ) & 0xFF ] << 16 ) ^
				( FSb[ ( RK[3] ) & 0xFF ] << 24 );

			RK[5] = RK[1] ^ RK[4];
			RK[6] = RK[2] ^ RK[5];
			RK[7] = RK[3] ^ RK[6];
		}
		break;

	case 12:

		for( i = 0; i < 8; i++, RK += 6 )
		{
			RK[6] = RK[0] ^ RCON[i] ^
				( FSb[ ( RK[5] >> 8 ) & 0xFF ] ) ^
				( FSb[ ( RK[5] >> 16 ) & 0xFF ] << 8 ) ^
				( FSb[ ( RK[5] >> 24 ) & 0xFF ] << 16 ) ^
				( FSb[ ( RK[5] ) & 0xFF ] << 24 );

			RK[7] = RK[1] ^ RK[6];
			RK[8] = RK[2] ^ RK[7];
			RK[9] = RK[3] ^ RK[8];
			RK[10] = RK[4] ^ RK[9];
			RK[11] = RK[5] ^ RK[10];
		}
		break;

	case 14:

		for( i = 0; i < 7; i++, RK += 8 )
		{
			RK[8] = RK[0] ^ RCON[i] ^
				( FSb[ ( RK[7] >> 8 ) & 0xFF ] ) ^
				( FSb[ ( RK[7] >> 16 ) & 0xFF ] << 8 ) ^
				( FSb[ ( RK[7] >> 24 ) & 0xFF ] << 16 ) ^
				( FSb[ ( RK[7] ) & 0xFF ] << 24 );

			RK[9] = RK[1] ^ RK[8];
			RK[10] = RK[2] ^ RK[9];
			RK[11] = RK[3] ^ RK[10];

			RK[12] = RK[4] ^
				( FSb[ ( RK[11] ) & 0xFF ] ) ^
				( FSb[ ( RK[11] >> 8 ) & 0xFF ] << 8 ) ^
				( FSb[ ( RK[11] >> 16 ) & 0xFF ] << 16 ) ^
				( FSb[ ( RK[11] >> 24 ) & 0xFF ] << 24 );

			RK[13] = RK[5] ^ RK[12];
			RK[14] = RK[6] ^ RK[13];
			RK[15] = RK[7] ^ RK[14];
		}
		break;

	default:

		break;
	}
	return 0;
}

/*
 * AES key schedule (decryption)
 */
int aes_setkey_dec(aes_context *ctx, const unsigned char *key, int keysize)
{
	int i, j;
	aes_context cty;
	unsigned long *RK;
	unsigned long *SK;

	switch( keysize )
	{
	case 128: ctx->nr = 10; break;
	case 192: ctx->nr = 12; break;
	case 256: ctx->nr = 14; break;
	default: return 1;
	}

#if defined(PADLOCK_ALIGN16)
	ctx->rk = RK = PADLOCK_ALIGN16( ctx->buf );
#else
	ctx->rk = RK = ctx->buf;
#endif

	i = aes_setkey_enc( &cty, key, keysize );
	if (i)
		return i;
	SK = cty.rk + cty.nr * 4;

	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;

	for( i = ctx->nr - 1, SK -= 8; i > 0; i--, SK -= 8 )
	{
		for( j = 0; j < 4; j++, SK++ )
		{
			*RK++ = RT0[ FSb[ ( *SK ) & 0xFF ] ] ^
				RT1[ FSb[ ( *SK >> 8 ) & 0xFF ] ] ^
				RT2[ FSb[ ( *SK >> 16 ) & 0xFF ] ] ^
				RT3[ FSb[ ( *SK >> 24 ) & 0xFF ] ];
		}
	}

	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;
	*RK++ = *SK++;

	memset( &cty, 0, sizeof( aes_context ) );
	return 0;
}

#define AES_FROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)	\
{						\
	X0 = *RK++ ^ FT0[ ( Y0 ) & 0xFF ] ^	\
		FT1[ ( Y1 >> 8 ) & 0xFF ] ^	\
		FT2[ ( Y2 >> 16 ) & 0xFF ] ^	\
		FT3[ ( Y3 >> 24 ) & 0xFF ];	\
						\
	X1 = *RK++ ^ FT0[ ( Y1 ) & 0xFF ] ^	\
		FT1[ ( Y2 >> 8 ) & 0xFF ] ^	\
		FT2[ ( Y3 >> 16 ) & 0xFF ] ^	\
		FT3[ ( Y0 >> 24 ) & 0xFF ];	\
						\
	X2 = *RK++ ^ FT0[ ( Y2 ) & 0xFF ] ^	\
		FT1[ ( Y3 >> 8 ) & 0xFF ] ^	\
		FT2[ ( Y0 >> 16 ) & 0xFF ] ^	\
		FT3[ ( Y1 >> 24 ) & 0xFF ];	\
						\
	X3 = *RK++ ^ FT0[ ( Y3 ) & 0xFF ] ^	\
		FT1[ ( Y0 >> 8 ) & 0xFF ] ^	\
		FT2[ ( Y1 >> 16 ) & 0xFF ] ^	\
		FT3[ ( Y2 >> 24 ) & 0xFF ];	\
}

#define AES_RROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)	\
{						\
	X0 = *RK++ ^ RT0[ ( Y0 ) & 0xFF ] ^	\
		RT1[ ( Y3 >> 8 ) & 0xFF ] ^	\
		RT2[ ( Y2 >> 16 ) & 0xFF ] ^	\
		RT3[ ( Y1 >> 24 ) & 0xFF ];	\
						\
	X1 = *RK++ ^ RT0[ ( Y1 ) & 0xFF ] ^	\
		RT1[ ( Y0 >> 8 ) & 0xFF ] ^	\
		RT2[ ( Y3 >> 16 ) & 0xFF ] ^	\
		RT3[ ( Y2 >> 24 ) & 0xFF ];	\
						\
	X2 = *RK++ ^ RT0[ ( Y2 ) & 0xFF ] ^	\
		RT1[ ( Y1 >> 8 ) & 0xFF ] ^	\
		RT2[ ( Y0 >> 16 ) & 0xFF ] ^	\
		RT3[ ( Y3 >> 24 ) & 0xFF ];	\
						\
	X3 = *RK++ ^ RT0[ ( Y3 ) & 0xFF ] ^	\
		RT1[ ( Y2 >> 8 ) & 0xFF ] ^	\
		RT2[ ( Y1 >> 16 ) & 0xFF ] ^	\
		RT3[ ( Y0 >> 24 ) & 0xFF ];	\
}

/*
 * AES-ECB block encryption/decryption
 */
void aes_crypt_ecb( aes_context *ctx,
	int mode,
	const unsigned char input[16],
	unsigned char output[16] )
{
	int i;
	unsigned long *RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

#if defined(XYSSL_PADLOCK_C) && defined(XYSSL_HAVE_X86)
	if( padlock_supports( PADLOCK_ACE ) )
	{
		if( padlock_xcryptecb( ctx, mode, input, output ) == 0 )
			return;
	}
#endif

	RK = ctx->rk;

	GET_ULONG_LE( X0, input, 0 ); X0 ^= *RK++;
	GET_ULONG_LE( X1, input, 4 ); X1 ^= *RK++;
	GET_ULONG_LE( X2, input, 8 ); X2 ^= *RK++;
	GET_ULONG_LE( X3, input, 12 ); X3 ^= *RK++;

	if( mode == AES_DECRYPT )
	{
		for( i = (ctx->nr >> 1) - 1; i > 0; i-- )
		{
			AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );
			AES_RROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );
		}

		AES_RROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );

		X0 = *RK++ ^ ( RSb[ ( Y0 ) & 0xFF ] ) ^
			( RSb[ ( Y3 >> 8 ) & 0xFF ] << 8 ) ^
			( RSb[ ( Y2 >> 16 ) & 0xFF ] << 16 ) ^
			( RSb[ ( Y1 >> 24 ) & 0xFF ] << 24 );

		X1 = *RK++ ^ ( RSb[ ( Y1 ) & 0xFF ] ) ^
			( RSb[ ( Y0 >>8 ) & 0xFF ] << 8 ) ^
			( RSb[ ( Y3 >> 16 ) & 0xFF ] << 16 ) ^
			( RSb[ ( Y2 >> 24 ) & 0xFF ] << 24 );

		X2 = *RK++ ^ ( RSb[ ( Y2 ) & 0xFF ] ) ^
			( RSb[ ( Y1 >> 8 ) & 0xFF ] << 8 ) ^
			( RSb[ ( Y0 >> 16 ) & 0xFF ] << 16 ) ^
			( RSb[ ( Y3 >> 24 ) & 0xFF ] << 24 );

		X3 = *RK++ ^ ( RSb[ ( Y3 ) & 0xFF ] ) ^
			( RSb[ ( Y2 >> 8 ) & 0xFF ] << 8 ) ^
			( RSb[ ( Y1 >> 16 ) & 0xFF ] << 16 ) ^
			( RSb[ ( Y0 >> 24 ) & 0xFF ] << 24 );
	}
	else /* AES_ENCRYPT */
	{
		for( i = (ctx->nr >> 1) - 1; i > 0; i-- )
		{
			AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );
			AES_FROUND( X0, X1, X2, X3, Y0, Y1, Y2, Y3 );
		}

		AES_FROUND( Y0, Y1, Y2, Y3, X0, X1, X2, X3 );

		X0 = *RK++ ^ ( FSb[ ( Y0 ) & 0xFF ] ) ^
			( FSb[ ( Y1 >> 8 ) & 0xFF ] << 8 ) ^
			( FSb[ ( Y2 >> 16 ) & 0xFF ] << 16 ) ^
			( FSb[ ( Y3 >> 24 ) & 0xFF ] << 24 );

		X1 = *RK++ ^ ( FSb[ ( Y1 ) & 0xFF ] ) ^
			( FSb[ ( Y2 >> 8 ) & 0xFF ] << 8 ) ^
			( FSb[ ( Y3 >> 16 ) & 0xFF ] << 16 ) ^
			( FSb[ ( Y0 >> 24 ) & 0xFF ] << 24 );

		X2 = *RK++ ^ ( FSb[ ( Y2 ) & 0xFF ] ) ^
			( FSb[ ( Y3 >> 8 ) & 0xFF ] << 8 ) ^
			( FSb[ ( Y0 >> 16 ) & 0xFF ] << 16 ) ^
			( FSb[ ( Y1 >> 24 ) & 0xFF ] << 24 );

		X3 = *RK++ ^ ( FSb[ ( Y3 ) & 0xFF ] ) ^
			( FSb[ ( Y0 >> 8 ) & 0xFF ] << 8 ) ^
			( FSb[ ( Y1 >> 16 ) & 0xFF ] << 16 ) ^
			( FSb[ ( Y2 >> 24 ) & 0xFF ] << 24 );
	}

	PUT_ULONG_LE( X0, output, 0 );
	PUT_ULONG_LE( X1, output, 4 );
	PUT_ULONG_LE( X2, output, 8 );
	PUT_ULONG_LE( X3, output, 12 );
}

/*
 * AES-CBC buffer encryption/decryption
 */
void aes_crypt_cbc( aes_context *ctx,
	int mode,
	int length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output )
{
	int i;
	unsigned char temp[16];

#if defined(XYSSL_PADLOCK_C) && defined(XYSSL_HAVE_X86)
	if( padlock_supports( PADLOCK_ACE ) )
	{
		if( padlock_xcryptcbc( ctx, mode, length, iv, input, output ) == 0 )
			return;
	}
#endif

	if( mode == AES_DECRYPT )
	{
		while( length > 0 )
		{
			memcpy( temp, input, 16 );
			aes_crypt_ecb( ctx, mode, input, output );

			for( i = 0; i < 16; i++ )
				output[i] = (unsigned char)( output[i] ^ iv[i] );

			memcpy( iv, temp, 16 );

			input += 16;
			output += 16;
			length -= 16;
		}
	}
	else
	{
		while( length > 0 )
		{
			for( i = 0; i < 16; i++ )
				output[i] = (unsigned char)( input[i] ^ iv[i] );

			aes_crypt_ecb( ctx, mode, output, output );
			memcpy( iv, output, 16 );

			input += 16;
			output += 16;
			length -= 16;
		}
	}
}

/*
 * AES-CFB buffer encryption/decryption
 */
void aes_crypt_cfb( aes_context *ctx,
	int mode,
	int length,
	int *iv_off,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output )
{
	int c, n = *iv_off;

	if( mode == AES_DECRYPT )
	{
		while( length-- )
		{
			if( n == 0 )
				aes_crypt_ecb( ctx, AES_ENCRYPT, iv, iv );

			c = *input++;
			*output++ = (unsigned char)( c ^ iv[n] );
			iv[n] = (unsigned char) c;

			n = (n + 1) & 0x0F;
		}
	}
	else
	{
		while( length-- )
		{
			if( n == 0 )
				aes_crypt_ecb( ctx, AES_ENCRYPT, iv, iv );

			iv[n] = *output++ = (unsigned char)( iv[n] ^ *input++ );

			n = (n + 1) & 0x0F;
		}
	}

	*iv_off = n;
}
