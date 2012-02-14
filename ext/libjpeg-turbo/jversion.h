/*
 * jversion.h
 *
 * Copyright (C) 1991-2010, Thomas G. Lane, Guido Vollbeding.
 * Copyright (C) 2010, 2012, D. R. Commander.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 * This file contains software version identification.
 */


#if JPEG_LIB_VERSION >= 80

#define JVERSION	"8b  16-May-2010"

#elif JPEG_LIB_VERSION >= 70

#define JVERSION        "7  27-Jun-2009"

#else

#define JVERSION	"6b  27-Mar-1998"

#endif

#define JCOPYRIGHT	"Copyright (C) 1991-2010 Thomas G. Lane, Guido Vollbeding\n" \
			"Copyright (C) 1999-2006 MIYASAKA Masaru\n" \
			"Copyright (C) 2009 Pierre Ossman for Cendio AB\n" \
			"Copyright (C) 2009-2012 D. R. Commander\n" \
			"Copyright (C) 2009-2011 Nokia Corporation and/or its subsidiary(-ies)"
