// Extracted from Bidi.cpp - version 26

// Reference implementation for Unicode Bidirectional Algorithm

// Bidi include file
#include "mupdf/fitz.h"
#include "bidi-imp.h"

#include <assert.h>

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

/*------------------------------------------------------------------------
	File: Bidi.Cpp

	Description
	-----------

	Sample Implementation of the Unicode Bidirectional Algorithm as it
	was revised by Revision 5 of the Unicode Technical Report # 9
	(1999-8-17)

	Verified for changes to the algorithm up through Unicode 5.2.0 (2009).

	This implementation is organized into several passes, each implemen-
	ting one or more of the rules of the Unicode Bidi Algorithm. The
	resolution of Weak Types and of Neutrals each use a state table
	approach.

	Both a printf based interface and a Windows DlgProc are provided for
	interactive testing.

	A stress harness comparing this implementation (v24) to a Java based
	implementation was used by Doug Felt to verify that the two
	implementations produce identical results for all strings up to six
	bidi classes and stochastic strings up to length 20.

	Version 26 was verified by the author against the Unicode 5.2.0
	file BidiTest.txt, which provides an exhaustive text of strings of
	length 4 or less, but covers some important cases where the language
	in UAX#9 had been clarified.

	To see this code running in an actual Windows program,
	download the free Unibook utility from http://unicode.org/unibook
	The bidi demo is executed from the tools menu. It is build from
	this source file.

	Build Notes
	-----------

	To compile the sample implementation please set the #define
	directives above so the correct headers get included. Not all the
	files are needed for all purposes. For the commandline version
	only bidi.h and bidi.cpp are needed.

	The Win32 version is provided as a dialog procedure. To use as a
	standalone program compile with the lbmain.cpp file. If all you
	need is the ability to run the code "as is", you can instead download
	the unibook utility from http://uincode.org/unibook/ which contains
	the bidi demo compiled from this source file.

	This code uses an extension to C++ that gives variables declared in
	a for() statement function the same scope as the for() statement.
	If your compiler does not support this extension, you may need to
	move the declaration, e.g. int ich = 0; in front of the for statement.

	Implementation Note
	-------------------

	NOTE: The Unicode Bidirectional Algorithm removes all explicit
		formatting codes in rule X9, but states that this can be
		simulated by conformant implementations. This implementation
		attempts to demonstrate such a simulation

		To demonstrate this, the current implementation does the
		following:

		in resolveExplicit()
			- change LRE, LRO, RLE, RLO, PDF to BN
			- assign nested levels to BN

		in resolveWeak and resolveNeutrals
			- assign L and R to BN's where they exist in place of
			  sor and eor by changing the last BN in front of a
			  level change to a strong type
			- skip over BN's for the purpose of determining actions
			- include BN in the count of deferred runs
				which will resolve some of them to EN, AN and N

		in resolveWhiteSpace
			- set the level of any surviving BN to the base level,
				or the level of the preceding character
			- include LRE,LRO, RLE, RLO, PDF and BN in the count
			   whitespace to be reset

		This will result in the same order for non-BN characters as
		if the BN characters had been removed.

		The clean() function can be used to remove boundary marks for
		verification purposes.

	Notation
	--------
	Pointer variables generally start with the letter p
	Counter variables generally start with the letter c
	Index variables generally start with the letter i
	Boolean variables generally start with the letter f

	The enumerated bidirectional types have the same name as in the
	description for the Unicode Bidirectional Algorithm

	Using this code outside a demo context
	--------------------------------------

	The way the functions are broken down in this demo code is based
	on the needs of the demo to show the evolution in internal state
	as the algorithm proceeds. This obscures how the algorithm would
	be used in practice. These are the steps:

	1. Allocate a pair of arrays large enough to hold bidi class
	   and calculated levels (one for each input character)

	2. Provide your own function to assign directional types (bidi
	   classes) corresponding to each character in the input,
	   conflating ON, WS, S to N. Unlike the classify function in this
	   demo, the input would be actual Unicode characters.

	3. Process the first paragraph by calling BidiParagraph. That
	   function changes B into BN and returns a length including the
	   paragraph separator. (The iteration over multiple paragraphs
	   is left as exercise for the reader).

	4. Assign directional types again, but now assign specific types
	   to whitespace characters.

	5. Instead of reordering the input in place it is often desirable
	   to calculate an array of offsets indicating the reordering.
	   To that end, allocate such an array here and use it instead
	   of the input array in the next step.

	6. Resolve and reorder the lines by calling BidiLines. That
	   function 'breaks' lines on LS characters. Provide an optional
	   array of flags indicating the location of other line breaks as
	   needed.

	Update History
	--------------
	Version 24 is the initial published and verified version of this
	reference implementation. Version 25 and its updates fix various
	minor issues with the scaffolding used for demonstrating the
	algorithm using pseudo-alphabets from the command line or dialog
	box. No changes to the implementation of the actual bidi algorithm
	are made in any of the minor updates to version 25. Version 26
	also makes no change to the actual algorithm but was verified
	against the official BidiTest.txt file for Unicode 5.2.0.

	- updated pseudo-alphabet

	- Last Revised 12-10-99 (25)

	- enable demo mode for release builds - no other changes

	- Last Revised 12-10-00 (25a)

	- fix regression in pseudo alphabet use for Windows UI

	- Last Revised 02-01-01 (25b)

	- fixed a few comments, renamed a variable

	- Last Revised 03-04-01 (25c)

	- make base level settable, enable mirror by default,
	fix dialog size

	- Last Revised 06-02-01 (25e)

	- fixed some comments

	- Last Revised 09-29-01 (25f)

	- fixed classification for LS,RLM,LRM in pseudo alphabet,
	focus issues in UI, regression fix to commandline from 25(e)
	fix DEMO switch

	- Last Revised 11-07-01 (25g)

	- fixed classification for plus/minus in pseudo alphabet
	to track changes made in Unicode 4.0.1

	- Last Revised 12-03-04 (25h)

	- now compiles as dialog-only program for WINDOWS_UI==1
	using new bidimain.cpp

	- Last Revised 12-02-07 (25i)

	- cleaned up whitespace and indenting in the source,
	fixed two comments (table headers)

	- Last Revised 15-03-07 (25j)

	- named enumerations

	- Last Revised 30-05-07 (25k)

	- added usage notes, minor edits to comments, indentation, etc
	throughout. Added the bidiParagraph function. Checked against
	changes in the Unicode Bidi Algorithm for Unicode 5.2.0. No
	changes needed to this implementation to match the values in
	the BidiTest.txt file in the Unicode Character Database.
	Minor fixes to dialog/windows proc, updated preprocessor directives.

	- Last Revised 03-08-09 (26)

	Credits:
	-------
	Written by: Asmus Freytag
	Command line interface by: Rick McGowan
	Verification (v24): Doug Felt

	Disclaimer and legal rights:
	---------------------------
	Copyright (C) 1999-2009, ASMUS, Inc. All Rights Reserved.
	Distributed under the Terms of Use in http://www.unicode.org/copyright.html.

	THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.
	IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
	BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES,
	OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
	WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
	ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE SOFTWARE.

	The file bid.rc is included in the software covered by the above.
------------------------------------------------------------------------*/

/* === HELPER FUNCTIONS AND DECLARATIONS ================================= */

#define odd(x) ((x) & 1)

/*----------------------------------------------------------------------
	The following array maps character codes to types for the purpose of
	this sample implementation. The legend string gives a human readable
	explanation of the pseudo alphabet.

	For simplicity, characters entered by buttons are given a 1:1 mapping
	between their type and pseudo character value. Pseudo characters that
	can be typed from the keyboard are explained in the legend string.

	Use the Unicode Character Database for the real values in real use.
 ---------------------------------------------------------------------*/

enum
{
	chLS = 0x15
};

#if 0
static const fz_bidi_chartype types_from_char[] =
{
// 0	   1	   2	   3	   4	   5	   6	   7	   8	   9	   a	   b	   c	   d	   e	   f
BDI_BN, BDI_BN, BDI_BN, BDI_BN, BDI_L,  BDI_R,  BDI_BN, BDI_BN, BDI_BN, BDI_S,  BDI_B,  BDI_S,  BDI_WS, BDI_B,  BDI_BN, BDI_BN, /*00-0f*/
BDI_LRO,BDI_LRE,BDI_PDF,BDI_RLO,BDI_RLE,BDI_WS, BDI_L,  BDI_R,  BDI_BN, BDI_BN, BDI_BN, BDI_BN, BDI_B,  BDI_B,  BDI_B,  BDI_S,  /*10-1f*/
BDI_WS, BDI_ON, BDI_ON, BDI_ET, BDI_ET, BDI_ET, BDI_ON, BDI_ON, BDI_ON, BDI_ON, BDI_ON, BDI_ES, BDI_CS, BDI_ES, BDI_CS, BDI_ES, /*20-2f*/
BDI_EN, BDI_EN, BDI_EN, BDI_EN, BDI_EN, BDI_EN, BDI_AN, BDI_AN, BDI_AN, BDI_AN, BDI_CS, BDI_ON, BDI_ON, BDI_ON, BDI_ON, BDI_ON, /*30-3f*/
BDI_ON, BDI_AL, BDI_AL, BDI_AL, BDI_AL, BDI_AL, BDI_AL, BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  /*40-4f*/
BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_R,  BDI_LRE,BDI_ON, BDI_RLE,BDI_PDF,BDI_S,  /*50-5f*/
BDI_NSM,BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  /*60-6f*/
BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_L,  BDI_LRO,BDI_B,  BDI_RLO,BDI_BN, BDI_ON, /*70-7f*/
};
#endif

/***************************************
	Reverse, human readable reference:

	LRM:	0x4
	RLM:	0x5
	  L:	0x16,a-z
	LRE:	0x11,[
	LRO:	0x10,{
	  R:	0x17,G-Z
	 AL:	A-F
	RLE:	0x14,]
	RLO:	0x13,}
	PDF:	0x12,^
	 EN:	0-5
	 ES:	/,+,[hyphen]
	 ET:	#,$,%
	 AN:	6-9
	 CS:	[comma],.,:
	NSM:	`
	 BN:	0x0-0x8,0xe,0xf,0x18-0x1b,~
	  B:	0xa,0xd,0x1c-0x1e,|
	  S:	0x9,0xb,0x1f,_
	 WS:	0xc,0x15,[space]
	 ON:	!,",&,',(,),*,;,<,=,>,?,@,\,0x7f
****************************************/

// === HELPER FUNCTIONS ================================================

#ifdef BIDI_LINE_AT_A_TIME
// reverse cch characters
static
void reverse(uint32_t *psz, int cch)
{
	uint32_t ch_temp;
	int ich;

	for (ich = 0; ich < --cch; ich++)
	{
		ch_temp = psz[ich];
		psz[ich] = psz[cch];
		psz[cch] = ch_temp;
	}
}
#endif

// Set a run of cval values at locations all prior to, but not including
// iStart, to the new value nval.
static
void set_deferred_run(fz_bidi_chartype *pval, size_t cval, size_t iStart, fz_bidi_chartype nval)
{
	size_t i;

	for (i = iStart; i > iStart - cval; )
	{
		pval[--i] = nval;
	}
}

static
void set_deferred_level_run(fz_bidi_level *pval, size_t cval, size_t iStart, fz_bidi_level nval)
{
	size_t i;

	for (i = iStart; i > iStart - cval; )
	{
		pval[--i] = nval;
	}
}

// === ASSIGNING BIDI CLASSES ============================================

// === THE PARAGRAPH LEVEL ===============================================

/*------------------------------------------------------------------------
	Function: resolve_paragraphs

	Resolves the input strings into blocks over which the algorithm
	is then applied.

	Implements Rule P1 of the Unicode Bidi Algorithm

	Input: Text string
		   Character count

	Output: revised character count

	Note:	This is a very simplistic function. In effect it restricts
			the action of the algorithm to the first paragraph in the input
			where a paragraph ends at the end of the first block separator
			or at the end of the input text.

------------------------------------------------------------------------*/
size_t fz_bidi_resolve_paragraphs(fz_bidi_chartype *types, size_t cch)
{
	size_t ich;

	// skip characters not of type B
	for(ich = 0; ich < cch && types[ich] != BDI_B; ich++)
		;
	// stop after first B, make it a BN for use in the next steps
	if (ich < cch && types[ich] == BDI_B)
		types[ich++] = BDI_BN;
	return ich;
}

#if 0
/*------------------------------------------------------------------------
	Function: base_level

	Determines the base level

	Implements rule P2 of the Unicode Bidi Algorithm.

	Input: Array of directional classes
		   Character count

	Note: Ignores explicit embeddings
------------------------------------------------------------------------*/
static int base_level(const fz_bidi_chartype *pcls, int cch)
{
	int ich;

	for (ich = 0; ich < cch; ich++)
	{
		switch (pcls[ich])
		{
		// strong left
		case BDI_L:
			return 0;

		// strong right
		case BDI_R:
		case BDI_AL:
			return 1;
		}
	}
	return 0;
}
#endif

//====== RESOLVE EXPLICIT ================================================

static fz_bidi_level greater_even(fz_bidi_level i)
{
	return odd(i) ? i + 1 : i + 2;
}

static fz_bidi_level greater_odd(fz_bidi_level i)
{
	return odd(i) ? i + 2 : i + 1;
}

static fz_bidi_chartype embedding_direction(fz_bidi_chartype level)
{
	return odd(level) ? BDI_R : BDI_L;
}

/*------------------------------------------------------------------------
	Function: resolveExplicit

	Recursively resolves explicit embedding levels and overrides.
	Implements rules X1-X9, of the Unicode Bidirectional Algorithm.

	Input: Base embedding level and direction
		   Character count

	Output: Array of embedding levels
		  Caller must allocate (one level per input character)

	In/Out: Array of direction classes

	Note: The function uses two simple counters to keep track of
		  matching explicit codes and PDF. Use the default argument for
		  the outermost call. The nesting counter counts the recursion
		  depth and not the embedding level.
------------------------------------------------------------------------*/
size_t fz_bidi_resolve_explicit(fz_bidi_level level, fz_bidi_chartype dir, fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch,
				fz_bidi_level n_nest)
{
	size_t ich;

	// always called with a valid nesting level
	// nesting levels are != embedding levels
	int nLastValid = n_nest;

	// check input values
	assert(n_nest >= 0 && level >= 0 && level <= BIDI_LEVEL_MAX);

	// process the text
	for (ich = 0; ich < cch; ich++)
	{
		fz_bidi_chartype cls = pcls[ich];
		switch (cls)
		{
		case BDI_LRO:
		case BDI_LRE:
			n_nest++;
			if (greater_even(level) <= BIDI_LEVEL_MAX)
			{
				plevel[ich] = greater_even(level);
				pcls[ich] = BDI_BN;
				ich += fz_bidi_resolve_explicit(plevel[ich], (cls == BDI_LRE ? BDI_N : BDI_L),
							&pcls[ich+1], &plevel[ich+1],
							 cch - (ich+1), n_nest);
				n_nest--;
				continue;
			}
			cls = pcls[ich] = BDI_BN;
			break;

		case BDI_RLO:
		case BDI_RLE:
			n_nest++;
			if (greater_odd(level) <= BIDI_LEVEL_MAX)
			{
				plevel[ich] = greater_odd(level);
				pcls[ich] = BDI_BN;
				ich += fz_bidi_resolve_explicit(plevel[ich], (cls == BDI_RLE ? BDI_N : BDI_R),
								&pcls[ich+1], &plevel[ich+1],
								 cch - (ich+1), n_nest);
				n_nest--;
				continue;
			}
			cls = pcls[ich] = BDI_BN;
			break;

		case BDI_PDF:
			cls = pcls[ich] = BDI_BN;
			if (n_nest)
			{
				if (nLastValid < n_nest)
				{
					n_nest--;
				}
				else
				{
					cch = ich; // break the loop, but complete body
				}
			}
			break;
		}

		// Apply the override
		if (dir != BDI_N)
		{
			cls = dir;
		}
		plevel[ich] = level;
		if (pcls[ich] != BDI_BN)
			pcls[ich] = cls;
	}

	return ich;
}

// === RESOLVE WEAK TYPES ================================================

enum bidi_state // possible states
{
	xa,	//	arabic letter
	xr,	//	right letter
	xl,	//	left letter

	ao,	//	arabic lett. foll by ON
	ro,	//	right lett. foll by ON
	lo,	//	left lett. foll by ON

	rt,	//	ET following R
	lt,	//	ET following L

	cn,	//	EN, AN following AL
	ra,	//	arabic number foll R
	re,	//	european number foll R
	la,	//	arabic number foll L
	le,	//	european number foll L

	ac,	//	CS following cn
	rc,	//	CS following ra
	rs,	//	CS,ES following re
	lc,	//	CS following la
	ls,	//	CS,ES following le

	ret,	//	ET following re
	let	//	ET following le
} ;

const unsigned char state_weak[][10] =
{
	//	N,  L,  R,  AN, EN, AL,NSM, CS, ES, ET,
/*xa*/  { ao, xl, xr, cn, cn, xa, xa, ao, ao, ao }, /* arabic letter		  */
/*xr*/  { ro, xl, xr, ra, re, xa, xr, ro, ro, rt }, /* right letter		   */
/*xl*/  { lo, xl, xr, la, le, xa, xl, lo, lo, lt }, /* left letter			  */

/*ao*/  { ao, xl, xr, cn, cn, xa, ao, ao, ao, ao }, /* arabic lett. foll by ON*/
/*ro*/  { ro, xl, xr, ra, re, xa, ro, ro, ro, rt }, /* right lett. foll by ON */
/*lo*/  { lo, xl, xr, la, le, xa, lo, lo, lo, lt }, /* left lett. foll by ON  */

/*rt*/  { ro, xl, xr, ra, re, xa, rt, ro, ro, rt }, /* ET following R		  */
/*lt*/  { lo, xl, xr, la, le, xa, lt, lo, lo, lt }, /* ET following L		  */

/*cn*/  { ao, xl, xr, cn, cn, xa, cn, ac, ao, ao }, /* EN, AN following AL	  */
/*ra*/  { ro, xl, xr, ra, re, xa, ra, rc, ro, rt }, /* arabic number foll R   */
/*re*/  { ro, xl, xr, ra, re, xa, re, rs, rs,ret }, /* european number foll R */
/*la*/  { lo, xl, xr, la, le, xa, la, lc, lo, lt }, /* arabic number foll L   */
/*le*/  { lo, xl, xr, la, le, xa, le, ls, ls,let }, /* european number foll L */

/*ac*/  { ao, xl, xr, cn, cn, xa, ao, ao, ao, ao }, /* CS following cn		  */
/*rc*/  { ro, xl, xr, ra, re, xa, ro, ro, ro, rt }, /* CS following ra		  */
/*rs*/  { ro, xl, xr, ra, re, xa, ro, ro, ro, rt }, /* CS,ES following re	  */
/*lc*/  { lo, xl, xr, la, le, xa, lo, lo, lo, lt }, /* CS following la		  */
/*ls*/  { lo, xl, xr, la, le, xa, lo, lo, lo, lt }, /* CS,ES following le	  */

/*ret*/ { ro, xl, xr, ra, re, xa,ret, ro, ro,ret }, /* ET following re		  */
/*let*/ { lo, xl, xr, la, le, xa,let, lo, lo,let }  /* ET following le		  */

};

enum bidi_action // possible actions
{
	// primitives
	IX = 0x100,				  // increment
	XX = 0xF,					// no-op

	// actions
	xxx = (XX << 4) + XX,		// no-op
	xIx = IX + xxx,			// increment run
	xxN = (XX << 4) + BDI_ON,	// set current to N
	xxE = (XX << 4) + BDI_EN,	// set current to EN
	xxA = (XX << 4) + BDI_AN,	// set current to AN
	xxR = (XX << 4) + BDI_R,	// set current to R
	xxL = (XX << 4) + BDI_L,	// set current to L
	Nxx = (BDI_ON << 4) + 0xF,	// set run to neutral
	Axx = (BDI_AN << 4) + 0xF,	// set run to AN
	ExE = (BDI_EN << 4) + BDI_EN,	// set run to EN, set current to EN
	NIx = (BDI_ON << 4) + 0xF + IX,	// set run to N, increment
	NxN = (BDI_ON << 4) + BDI_ON,	// set run to N, set current to N
	NxR = (BDI_ON << 4) + BDI_R,	// set run to N, set current to R
	NxE = (BDI_ON << 4) + BDI_EN,	// set run to N, set current to EN

	AxA = (BDI_AN << 4) + BDI_AN,	// set run to AN, set current to AN
	NxL = (BDI_ON << 4) + BDI_L,	// set run to N, set current to L
	LxL = (BDI_L << 4) + BDI_L	// set run to L, set current to L
};

typedef uint16_t fz_bidi_action;

const fz_bidi_action action_weak[][10] =
{
	//   N,.. L,   R,  AN,  EN,  AL, NSM,  CS,..ES,  ET,
/*xa*/ { xxx, xxx, xxx, xxx, xxA, xxR, xxR, xxN, xxN, xxN }, /* arabic letter			*/
/*xr*/ { xxx, xxx, xxx, xxx, xxE, xxR, xxR, xxN, xxN, xIx }, /* right letter			 */
/*xl*/ { xxx, xxx, xxx, xxx, xxL, xxR, xxL, xxN, xxN, xIx }, /* left letter			 */

/*ao*/ { xxx, xxx, xxx, xxx, xxA, xxR, xxN, xxN, xxN, xxN }, /* arabic lett. foll by ON	*/
/*ro*/ { xxx, xxx, xxx, xxx, xxE, xxR, xxN, xxN, xxN, xIx }, /* right lett. foll by ON	*/
/*lo*/ { xxx, xxx, xxx, xxx, xxL, xxR, xxN, xxN, xxN, xIx }, /* left lett. foll by ON	*/

/*rt*/ { Nxx, Nxx, Nxx, Nxx, ExE, NxR, xIx, NxN, NxN, xIx }, /* ET following R			*/
/*lt*/ { Nxx, Nxx, Nxx, Nxx, LxL, NxR, xIx, NxN, NxN, xIx }, /* ET following L			*/

/*cn*/ { xxx, xxx, xxx, xxx, xxA, xxR, xxA, xIx, xxN, xxN }, /* EN, AN following  AL	*/
/*ra*/ { xxx, xxx, xxx, xxx, xxE, xxR, xxA, xIx, xxN, xIx }, /* arabic number foll R	*/
/*re*/ { xxx, xxx, xxx, xxx, xxE, xxR, xxE, xIx, xIx, xxE }, /* european number foll R	*/
/*la*/ { xxx, xxx, xxx, xxx, xxL, xxR, xxA, xIx, xxN, xIx }, /* arabic number foll L	*/
/*le*/ { xxx, xxx, xxx, xxx, xxL, xxR, xxL, xIx, xIx, xxL }, /* european number foll L	*/

/*ac*/ { Nxx, Nxx, Nxx, Axx, AxA, NxR, NxN, NxN, NxN, NxN }, /* CS following cn		 */
/*rc*/ { Nxx, Nxx, Nxx, Axx, NxE, NxR, NxN, NxN, NxN, NIx }, /* CS following ra		 */
/*rs*/ { Nxx, Nxx, Nxx, Nxx, ExE, NxR, NxN, NxN, NxN, NIx }, /* CS,ES following re		*/
/*lc*/ { Nxx, Nxx, Nxx, Axx, NxL, NxR, NxN, NxN, NxN, NIx }, /* CS following la		 */
/*ls*/ { Nxx, Nxx, Nxx, Nxx, LxL, NxR, NxN, NxN, NxN, NIx }, /* CS,ES following le		*/

/*ret*/{ xxx, xxx, xxx, xxx, xxE, xxR, xxE, xxN, xxN, xxE }, /* ET following re			*/
/*let*/{ xxx, xxx, xxx, xxx, xxL, xxR, xxL, xxN, xxN, xxL }  /* ET following le			*/
};

static
fz_bidi_chartype get_deferred_type(fz_bidi_action action)
{
	return (action >> 4) & 0xF;
}

static
fz_bidi_chartype get_resolved_type(fz_bidi_action action)
{
	return action & 0xF;
}

/* Note on action table:

	States can be of two kinds:
	 - Immediate Resolution State, where each input token
	   is resolved as soon as it is seen. These states have
	   only single action codes (xxN) or the no-op (xxx)
	   for static input tokens.
	 - Deferred Resolution State, where input tokens either
	   either extend the run (xIx) or resolve its Type (e.g. Nxx).

	Input classes are of three kinds
	 - Static Input Token, where the class of the token remains
	   unchanged on output (AN, L, N, R)
	 - Replaced Input Token, where the class of the token is
	   always replaced on output (AL, BDI_BN, NSM, CS, ES, ET)
	 - Conditional Input Token, where the class of the token is
	   changed on output in some but not all cases (EN)

	 Where tokens are subject to change, a double action
	 (e.g. NxA, or NxN) is _required_ after deferred states,
	 resolving both the deferred state and changing the current token.

	These properties of the table are verified by assertions below.
	This code is needed only during debugging and maintenance
*/

/*------------------------------------------------------------------------
	Function: resolveWeak

	Resolves the directionality of numeric and other weak character types

	Implements rules X10 and W1-W6 of the Unicode Bidirectional Algorithm.

	Input: Array of embedding levels
		   Character count

	In/Out: Array of directional classes

	Note: On input only these directional classes are expected
		  AL, HL, R, L,  ON, BDI_BN, NSM, AN, EN, ES, ET, CS,
------------------------------------------------------------------------*/
void fz_bidi_resolve_weak(fz_context *ctx, fz_bidi_level baselevel, fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch)
{
	int state = odd(baselevel) ? xr : xl;
	fz_bidi_chartype cls;
	size_t ich;
	fz_bidi_action action;
	fz_bidi_chartype cls_run;
	fz_bidi_chartype cls_new;

	fz_bidi_level level = baselevel;

	size_t cch_run = 0;

	for (ich = 0; ich < cch; ich++)
	{
		if (pcls[ich] > BDI_BN) {
			fz_warn(ctx, "error: pcls[%zu] > BN (%d)\n", ich, pcls[ich]);
		}

		// ignore boundary neutrals
		if (pcls[ich] == BDI_BN)
		{
			// must flatten levels unless at a level change;
			plevel[ich] = level;

			// lookahead for level changes
			if (ich + 1 == cch && level != baselevel)
			{
				// have to fixup last BN before end of the loop, since
				// its fix-upped value will be needed below the assert
				pcls[ich] = embedding_direction(level);
			}
			else if (ich + 1 < cch && level != plevel[ich+1] && pcls[ich+1] != BDI_BN)
			{
				// fixup LAST BN in front / after a level run to make
				// it act like the SOR/EOR in rule X10
				int newlevel = plevel[ich+1];
				if (level > newlevel) {
					newlevel = level;
				}
				plevel[ich] = newlevel;

				// must match assigned level
				pcls[ich] = embedding_direction(newlevel);
				level = plevel[ich+1];
			}
			else
			{
				// don't interrupt runs
				if (cch_run)
				{
					cch_run++;
				}
				continue;
			}
		}

		assert(pcls[ich] <= BDI_BN);
		cls = pcls[ich];

		action = action_weak[state][cls];

		// resolve the directionality for deferred runs
		cls_run = get_deferred_type(action);
		if (cls_run != XX)
		{
			set_deferred_run(pcls, cch_run, ich, cls_run);
			cch_run = 0;
		}

		// resolve the directionality class at the current location
		cls_new = get_resolved_type(action);
		if (cls_new != XX)
			pcls[ich] = cls_new;

		// increment a deferred run
		if (IX & action)
			cch_run++;

		state = state_weak[state][cls];
	}

	// resolve any deferred runs
	// use the direction of the current level to emulate PDF
	cls = embedding_direction(level);

	// resolve the directionality for deferred runs
	cls_run = get_deferred_type(action_weak[state][cls]);
	if (cls_run != XX)
		set_deferred_run(pcls, cch_run, ich, cls_run);
}

// === RESOLVE NEUTRAL TYPES ==============================================

// action values
enum neutral_action
{
	// action to resolve previous input
	nL = BDI_L,		// resolve EN to L
	En = 3 << 4,		// resolve neutrals run to embedding level direction
	Rn = BDI_R << 4,	// resolve neutrals run to strong right
	Ln = BDI_L << 4,	// resolved neutrals run to strong left
	In = (1<<8),		// increment count of deferred neutrals
	LnL = (1<<4)+BDI_L	// set run and EN to L
};

static fz_bidi_chartype
get_deferred_neutrals(fz_bidi_action action, fz_bidi_level level)
{
	action = (action >> 4) & 0xF;
	if (action == (En >> 4))
		return embedding_direction(level);
	else
		return action;
}

static fz_bidi_chartype get_resolved_neutrals(fz_bidi_action action)
{
	action = action & 0xF;
	if (action == In)
		return 0;
	else
		return action;
}

// state values
enum neutral_state
{
	// new temporary class
	r,	// R and characters resolved to R
	l,	// L and characters resolved to L
	rn,	// N preceded by right
	ln,	// N preceded by left
	a,	// AN preceded by left (the abbrev 'la' is used up above)
	na	// N preceded by a
} ;

/*------------------------------------------------------------------------
	Notes:

	By rule W7, whenever a EN is 'dominated' by an L (including start of
	run with embedding direction = L) it is resolved to, and further treated
	as L.

	This leads to the need for 'a' and 'na' states.
------------------------------------------------------------------------*/

const int action_neutrals[][5] =
{
//	N,	L,	R, AN, EN, = cls
					// state =
	{In,  0,  0,  0,  0},		// r	right
	{In,  0,  0,  0,  BDI_L},	// l	left

	{In, En, Rn, Rn, Rn},		// rn	N preceded by right
	{In, Ln, En, En, LnL},		// ln	N preceded by left

	{In,  0,  0,  0,  BDI_L},	// a   AN preceded by left
	{In, En, Rn, Rn, En}		// na	N  preceded by a
} ;

const int state_neutrals[][5] =
{
//	 N, L,	R,	AN, EN = cls
					// state =
	{rn, l,	r,	r,	r},	// r   right
	{ln, l,	r,	a,	l},	// l   left

	{rn, l,	r,	r,	r},	// rn  N preceded by right
	{ln, l,	r,	a,	l},	// ln  N preceded by left

	{na, l,	r,	a,	l},	// a  AN preceded by left
	{na, l,	r,	a,	l}	// na  N preceded by la
} ;

/*------------------------------------------------------------------------
	Function: resolveNeutrals

	Resolves the directionality of neutral character types.

	Implements rules W7, N1 and N2 of the Unicode Bidi Algorithm.

	Input: Array of embedding levels
		   Character count
		   Baselevel

	In/Out: Array of directional classes

	Note: On input only these directional classes are expected
		  R,  L,  N, AN, EN and BN

		  W8 resolves a number of ENs to L
------------------------------------------------------------------------*/
void fz_bidi_resolve_neutrals(fz_bidi_level baselevel, fz_bidi_chartype *pcls, const fz_bidi_level *plevel, size_t cch)
{
	// the state at the start of text depends on the base level
	int state = odd(baselevel) ? r : l;
	fz_bidi_chartype cls;
	size_t ich;
	fz_bidi_chartype cls_run;

	size_t cch_run = 0;
	fz_bidi_level level = baselevel;

	for (ich = 0; ich < cch; ich++)
	{
		int action;
		fz_bidi_chartype cls_new;

		// ignore boundary neutrals
		if (pcls[ich] == BDI_BN)
		{
			// include in the count for a deferred run
			if (cch_run)
				cch_run++;

			// skip any further processing
			continue;
		}

		assert(pcls[ich] < 5); // "Only N, L, R, AN, EN are allowed"
		cls = pcls[ich];

		action = action_neutrals[state][cls];

		// resolve the directionality for deferred runs
		cls_run = get_deferred_neutrals(action, level);
		if (cls_run != BDI_N)
		{
			set_deferred_run(pcls, cch_run, ich, cls_run);
			cch_run = 0;
		}

		// resolve the directionality class at the current location
		cls_new = get_resolved_neutrals(action);
		if (cls_new != BDI_N)
			pcls[ich] = cls_new;

		if (In & action)
			cch_run++;

		state = state_neutrals[state][cls];
		level = plevel[ich];
	}

	// resolve any deferred runs
	cls = embedding_direction(level);	// eor has type of current level

	// resolve the directionality for deferred runs
	cls_run = get_deferred_neutrals(action_neutrals[state][cls], level);
	if (cls_run != BDI_N)
		set_deferred_run(pcls, cch_run, ich, cls_run);
}

// === RESOLVE IMPLICITLY =================================================

/*------------------------------------------------------------------------
	Function: resolveImplicit

	Recursively resolves implicit embedding levels.
	Implements rules I1 and I2 of the Unicode Bidirectional Algorithm.

	Input: Array of direction classes
		   Character count
		   Base level

	In/Out: Array of embedding levels

	Note: levels may exceed 15 on output.
		  Accepted subset of direction classes
		  R, L, AN, EN
------------------------------------------------------------------------*/
const fz_bidi_level add_level[][4] =
{
	// L,  R,	AN, EN = cls
					// level =
/* even */ { 0,	1,	2,	2 },	// EVEN
/* odd	*/ { 1,	0,	1,	1 }	// ODD

};

void fz_bidi_resolve_implicit(const fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch)
{
	size_t ich;

	for (ich = 0; ich < cch; ich++)
	{
		// cannot resolve bn here, since some bn were resolved to strong
		// types in resolveWeak. To remove these we need the original
		// types, which are available again in resolveWhiteSpace
		if (pcls[ich] == BDI_BN)
		{
			continue;
		}
		assert(pcls[ich] > 0); // "No Neutrals allowed to survive here."
		assert(pcls[ich] < 5); // "Out of range."
		plevel[ich] += add_level[odd(plevel[ich])][pcls[ich] - 1];
	}
}

#if 0
// === REORDER ===========================================================
/*------------------------------------------------------------------------
	Function: resolve_lines

	Breaks a paragraph into lines

	Input:	Character count
			Array of line break flags
	In/Out:	Array of characters

	Returns the count of characters on the first line

	Note: This function only breaks lines at hard line breaks. Other
	line breaks can be passed in. If pbrk[n] is true, then a break
	occurs after the character in psz_input[n]. Breaks before the first
	character are not allowed.
------------------------------------------------------------------------*/
static int resolve_lines(uint32_t *psz_input, int *pbrk, int cch)
{
	int ich;

	// skip characters not of type LS
	for(ich = 0; ich < cch; ich++)
	{
		if (psz_input[ich] == chLS || (pbrk && pbrk[ich]))
		{
			ich++;
			break;
		}
	}

	return ich;
}
#endif

/*------------------------------------------------------------------------
	Function: fz_bidi_resolve_whitespace

	Resolves levels for WS and S
	Implements rule L1 of the Unicode bidi Algorithm.

	Input:	Base embedding level
			Character count
			Array of direction classes (for one line of text)

	In/Out: Array of embedding levels (for one line of text)

	Note: this should be applied a line at a time. The default driver
		  code supplied in this file assumes a single line of text; for
		  a real implementation, cch and the initial pointer values
		  would have to be adjusted.
------------------------------------------------------------------------*/
void fz_bidi_resolve_whitespace(fz_bidi_level baselevel, const fz_bidi_chartype *pcls, fz_bidi_level *plevel,
				size_t cch)
{
	size_t cchrun = 0;
	fz_bidi_level oldlevel = baselevel;
	size_t ich;

	for (ich = 0; ich < cch; ich++)
	{
		switch(pcls[ich])
		{
		default:
			cchrun = 0; // any other character breaks the run
			break;
		case BDI_WS:
			cchrun++;
			break;

		case BDI_RLE:
		case BDI_LRE:
		case BDI_LRO:
		case BDI_RLO:
		case BDI_PDF:
		case BDI_BN:
			plevel[ich] = oldlevel;
			cchrun++;
			break;

		case BDI_S:
		case BDI_B:
			// reset levels for WS before eot
			set_deferred_level_run(plevel, cchrun, ich, baselevel);
			cchrun = 0;
			plevel[ich] = baselevel;
			break;
		}
		oldlevel = plevel[ich];
	}
	// reset level before eot
	set_deferred_level_run(plevel, cchrun, ich, baselevel);
}

#ifdef BIDI_LINE_AT_A_TIME
/*------------------------------------------------------------------------
	Functions: reorder/reorderLevel

	Recursively reorders the display string
	"From the highest level down, reverse all characters at that level and
	higher, down to the lowest odd level"

	Implements rule L2 of the Unicode bidi Algorithm.

	Input: Array of embedding levels
		   Character count
		   Flag enabling reversal (set to false by initial caller)

	In/Out: Text to reorder

	Note: levels may exceed 15 resp. 61 on input.

	Rule L3 - reorder combining marks is not implemented here
	Rule L4 - glyph mirroring is implemented as a display option below

	Note: this should be applied a line at a time
-------------------------------------------------------------------------*/
static int reorderLevel(fz_bidi_level level, uint32_t *psz_text, const fz_bidi_level *plevel, int cch,
				 int f_reverse)
{
	int ich;

	// true as soon as first odd level encountered
	f_reverse = f_reverse || odd(level);

	for (ich = 0; ich < cch; ich++)
	{
		if (plevel[ich] < level)
		{
			break;
		}
		else if (plevel[ich] > level)
		{
			ich += reorderLevel(level + 1, psz_text + ich, plevel + ich,
				cch - ich, f_reverse) - 1;
		}
	}
	if (f_reverse)
	{
		reverse(psz_text, ich);
	}
	return ich;
}

int Bidi_reorder(fz_bidi_level baselevel, uint32_t *psz_text, const fz_bidi_level *plevel, int cch)
{
	int ich = 0;

	while (ich < cch)
	{
		ich += reorderLevel(baselevel, psz_text + ich, plevel + ich,
			cch - ich, FALSE);
	}
	return ich;
}
#endif
