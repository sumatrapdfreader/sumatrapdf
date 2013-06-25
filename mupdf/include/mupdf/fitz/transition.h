#ifndef MUPDF_FITZ_TRANSITION_H
#define MUPDF_FITZ_TRANSITION_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/pixmap.h"

/* Transition support */
typedef struct fz_transition_s fz_transition;

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

struct fz_transition_s
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
};

/*
	fz_generate_transition: Generate a frame of a transition.

	tpix: Target pixmap
	opix: Old pixmap
	npix: New pixmap
	time: Position within the transition (0 to 256)
	trans: Transition details

	Returns 1 if successfully generated a frame.
*/
int fz_generate_transition(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time, fz_transition *trans);

#endif
