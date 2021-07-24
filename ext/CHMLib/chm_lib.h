/***************************************************************************
 *             chm_lib.h - CHM archive manipulation routines               *
 *                           -------------------                           *
 *                                                                         *
 *  author:     Jed Wing <jedwin@ugcs.caltech.edu>                         *
 *  version:    0.3                                                        *
 *  notes:      These routines are meant for the manipulation of microsoft *
 *              .chm (compiled html help) files, but may likely be used    *
 *              for the manipulation of any ITSS archive, if ever ITSS     *
 *              archives are used for any other purpose.                   *
 *                                                                         *
 *              Note also that the section names are statically handled.   *
 *              To be entirely correct, the section names should be read   *
 *              from the section names meta-file, and then the various     *
 *              content sections and the "transforms" to apply to the data *
 *              they contain should be inferred from the section name and  *
 *              the meta-files referenced using that name; however, all of *
 *              the files I've been able to get my hands on appear to have *
 *              only two sections: Uncompressed and MSCompressed.          *
 *              Additionally, the ITSS.DLL file included with Windows does *
 *              not appear to handle any different transforms than the     *
 *              simple LZX-transform.  Furthermore, the list of transforms *
 *              to apply is broken, in that only half the required space   *
 *              is allocated for the list.  (It appears as though the      *
 *              space is allocated for ASCII strings, but the strings are  *
 *              written as unicode.  As a result, only the first half of   *
 *              the string appears.)  So this is probably not too big of   *
 *              a deal, at least until CHM v4 (MS .lit files), which also  *
 *              incorporate encryption, of some description.               *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License as        *
 *   published by the Free Software Foundation; either version 2.1 of the  *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#ifndef INCLUDED_CHMLIB_H
#define INCLUDED_CHMLIB_H

#ifdef __cplusplus
extern "C" {
#endif

/* the two available spaces in a CHM file                      */
/* N.B.: The format supports arbitrarily many spaces, but only */
/*       two appear to be used at present.                     */
#define CHM_UNCOMPRESSED (0)
#define CHM_COMPRESSED (1)

/* structure representing an ITS (CHM) file stream             */
struct chmFile;

/* structure representing an element from an ITS file stream   */
#define CHM_MAX_PATHLEN (512)
struct chmUnitInfo {
    uint64_t start;
    uint64_t length;
    int space;
    int flags;
    char path[CHM_MAX_PATHLEN + 1];
};

/* open an ITS archive */
struct chmFile* chm_open(const char* data, size_t len);

/* close an ITS archive */
void chm_close(struct chmFile* h);

/* methods for ssetting tuning parameters for particular file */
#define CHM_PARAM_MAX_BLOCKS_CACHED 0
void chm_set_param(struct chmFile* h, int paramType, int paramVal);

/* resolve a particular object from the archive */
#define CHM_RESOLVE_SUCCESS (0)
#define CHM_RESOLVE_FAILURE (1)
int chm_resolve_object(struct chmFile* h, const char* objPath, struct chmUnitInfo* ui);

/* retrieve part of an object from the archive */
int64_t chm_retrieve_object(struct chmFile* h, struct chmUnitInfo* ui, uint8_t* buf, uint64_t addr, int64_t len);

/* enumerate the objects in the .chm archive */
typedef int (*CHM_ENUMERATOR)(struct chmFile* h, struct chmUnitInfo* ui, void* context);
#define CHM_ENUMERATE_NORMAL (1)
#define CHM_ENUMERATE_META (2)
#define CHM_ENUMERATE_SPECIAL (4)
#define CHM_ENUMERATE_FILES (8)
#define CHM_ENUMERATE_DIRS (16)
#define CHM_ENUMERATE_ALL (31)
#define CHM_ENUMERATOR_FAILURE (0)
#define CHM_ENUMERATOR_CONTINUE (1)
#define CHM_ENUMERATOR_SUCCESS (2)
int chm_enumerate(struct chmFile* h, int what, CHM_ENUMERATOR e, void* context);

int chm_enumerate_dir(struct chmFile* h, const char* prefix, int what, CHM_ENUMERATOR e, void* context);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_CHMLIB_H */
