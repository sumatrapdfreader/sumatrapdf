/*  For use with Bidi Reference Implementation
    For more information see the associated file bidi-std.c

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
*/

/* Bidirectional Character Types
 * as defined by the Unicode Bidirectional Algorithm Table 3-7.
 * The list of bidirectional character types here is not grouped the
 * same way as the table 3-7, since the numeric values for the types
 * are chosen to keep the state and action tables compact.
 */
enum
{
	/* input types */
			/* ON MUST be zero, code relies on ON = N = 0 */
	BDI_ON = 0,	/* Other Neutral  */
	BDI_L,		/* Left-to-right Letter */
	BDI_R,		/* Right-to-left Letter */
	BDI_AN,		/* Arabic Number */
	BDI_EN,		/* European Number */
	BDI_AL,		/* Arabic Letter (Right-to-left) */
	BDI_NSM,	/* Non-spacing Mark */
	BDI_CS,		/* Common Separator */
	BDI_ES,		/* European Separator */
	BDI_ET,		/* European Terminator (post/prefix e.g. $ and %) */

	/* resolved types */
	BDI_BN,		/* Boundary neutral (type of RLE etc after explicit levels)*/

	/* input types, */
	BDI_S,		/* Segment Separator (TAB)	used only in L1 */
	BDI_WS,		/* White space			used only in L1 */
	BDI_B,		/* Paragraph Separator (aka as PS) */

	/* types for explicit controls */
	BDI_RLO,	/* these are used only in X1-X9 */
	BDI_RLE,
	BDI_LRO,
	BDI_LRE,
	BDI_PDF,

	/* resolved types, also resolved directions */
	BDI_N = BDI_ON	/* alias, where ON, WS and S are treated the same */
};

typedef int fz_bidi_level; /* Note: Max level is 125 */
typedef uint8_t fz_bidi_chartype;

enum
{
	BIDI_LEVEL_MAX = 125 /* Updated for 6.3.0 */
};

void fz_bidi_resolve_neutrals(fz_bidi_level baselevel, fz_bidi_chartype *pcls, const fz_bidi_level *plevel, size_t cch);
void fz_bidi_resolve_implicit(const fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch);
void fz_bidi_resolve_weak(fz_context *ctx, fz_bidi_level baselevel, fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch);
void fz_bidi_resolve_whitespace(fz_bidi_level baselevel, const fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch);
size_t fz_bidi_resolve_explicit(fz_bidi_level level, fz_bidi_chartype dir, fz_bidi_chartype *pcls, fz_bidi_level *plevel, size_t cch, fz_bidi_level nNest);
size_t fz_bidi_resolve_paragraphs(fz_bidi_chartype *types, size_t cch);
