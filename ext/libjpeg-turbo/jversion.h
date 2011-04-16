/*
 * jversion.h
 *
 * Copyright (C) 1991-2010, Thomas G. Lane, Guido Vollbeding.
 * Copyright (C) 2010, D. R. Commander.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains software version identification.
 */


#if JPEG_LIB_VERSION >= 80

#define JVERSION	"8b  16-May-2010"

#define JCOPYRIGHT	"Copyright (C) 2010, Thomas G. Lane, Guido Vollbeding"

#elif JPEG_LIB_VERSION >= 70

#define JVERSION        "7  27-Jun-2009"

#define JCOPYRIGHT      "Copyright (C) 2009, Thomas G. Lane, Guido Vollbeding"

#else

#define JVERSION	"6b  27-Mar-1998"

#define JCOPYRIGHT	"Copyright (C) 1998, Thomas G. Lane"

#endif

#define LJTCOPYRIGHT	"Copyright (C) 1999-2006 MIYASAKA Masaru\n" \
			"Copyright (C) 2004 Landmark Graphics Corporation\n" \
			"Copyright (C) 2005-2007 Sun Microsystems, Inc.\n" \
			"Copyright (C) 2009 Pierre Ossman for Cendio AB\n" \
			"Copyright (C) 2009-2011 D. R. Commander"
