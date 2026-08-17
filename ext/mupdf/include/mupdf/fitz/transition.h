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

#ifndef MUPDF_FITZ_TRANSITION_H
#define MUPDF_FITZ_TRANSITION_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/pixmap.h"

/* Transition support */
enum {
	FZ_TRANSITION_NONE = 0, /* aka 'R' or 'REPLACE' */
	FZ_TRANSITION_SPLIT,
	FZ_TRANSITION_BLINDS,
	FZ_TRANSITION_BOX,
	FZ_TRANSITION_WIPE,
	FZ_TRANSITION_DISSOLVE,
	FZ_TRANSITION_GLITTER,
	FZ_TRANSITION_FLY,
	FZ_TRANSITION_PUSH,
	FZ_TRANSITION_COVER,
	FZ_TRANSITION_UNCOVER,
	FZ_TRANSITION_FADE
};

typedef struct
{
	int type;
	float duration; /* Effect duration (seconds) */

	/* Parameters controlling the effect */
	int vertical; /* 0 or 1 */
	int outwards; /* 0 or 1 */
	int direction; /* Degrees */
	/* Potentially more to come */

	/* State variables for use of the transition code */
	int state0;
	int state1;
} fz_transition;

/**
	Generate a frame of a transition.

	tpix: Target pixmap
	opix: Old pixmap
	npix: New pixmap
	time: Position within the transition (0 to 256)
	trans: Transition details

	Returns 1 if successfully generated a frame.

	Note: Pixmaps must include alpha.
*/
int fz_generate_transition(fz_context *ctx, fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time, fz_transition *trans);

#endif
