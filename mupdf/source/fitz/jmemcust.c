/*
 * Copyright (C) 2001-2017 Artifex Software, Inc.
 * All Rights Reserved.
 *
 * This software is provided AS-IS with no warranty, either express or
 * implied.
 *
 * This software is distributed under license and may not be copied,
 * modified or distributed except as expressly authorized under the terms
 * of the license contained in the file LICENSE in this distribution.
 *
 * Refer to licensing information at http://www.artifex.com or contact
 * Artifex Software, Inc., 7 Mt. Lassen Drive - Suite A-134, San Rafael,
 * CA 94903, U.S.A., +1(415)492-9861, for further information.
 */

#if !defined(SHARE_JPEG) || SHARE_JPEG==0

#include "jinclude.h"
#include "jpeglib.h"
#include "jmemsys.h"
#include "jerror.h"
#include "jmemcust.h"

GLOBAL(void *)
jpeg_get_small (j_common_ptr cinfo, size_t sizeofobject)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);

	return (void *) (cmem->j_mem_get_small)(cinfo, sizeofobject);
}

GLOBAL(void)
jpeg_free_small (j_common_ptr cinfo, void * object, size_t sizeofobject)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);

	(cmem->j_mem_free_small)(cinfo, object, sizeofobject);
}

/*
 * "Large" objects are treated the same as "small" ones.
 * NB: although we include FAR keywords in the routine declarations,
 * this file won't actually work in 80x86 small/medium model; at least,
 * you probably won't be able to process useful-size images in only 64KB.
 */

GLOBAL(void FAR *)
jpeg_get_large (j_common_ptr cinfo, size_t sizeofobject)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);

	return (void *) (cmem->j_mem_get_large)(cinfo, sizeofobject);
}

GLOBAL(void)
jpeg_free_large (j_common_ptr cinfo, void FAR * object, size_t sizeofobject)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);

	(cmem->j_mem_free_large)(cinfo, object, sizeofobject);
}

/*
 * This routine computes the total memory space available for allocation.
 * Here we always say, "we got all you want bud!"
 */

GLOBAL(long)
jpeg_mem_available (j_common_ptr cinfo, long min_bytes_needed,
		long max_bytes_needed, long already_allocated)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);
	long ret = max_bytes_needed;

	if (cmem->j_mem_avail)
		ret = (cmem->j_mem_avail)(cinfo);

	return ret;
}

/*
 * Backing store (temporary file) management.
 * Since jpeg_mem_available always promised the moon,
 * this should never be called and we can just error out.
 */

GLOBAL(void)
jpeg_open_backing_store (j_common_ptr cinfo, backing_store_ptr info,
		long total_bytes_needed)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);

	if (cmem->j_mem_open_backing_store) {
		(cmem->j_mem_open_backing_store)(cinfo, info, total_bytes_needed);
	}
	else
		ERREXIT(cinfo, JERR_NO_BACKING_STORE);
}

/*
 * These routines take care of any system-dependent initialization and
 * cleanup required. Here, there isn't any.
 */

GLOBAL(long)
jpeg_mem_init (j_common_ptr cinfo)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);
	long ret = 0;

	if (cmem->j_mem_init)
		ret = (cmem->j_mem_init)(cinfo);

	return ret;
}

GLOBAL(void)
jpeg_mem_term (j_common_ptr cinfo)
{
	jpeg_cust_mem_data *cmem = GET_CUST_MEM_DATA(cinfo);

	if (cmem->j_mem_term)
		(cmem->j_mem_term)(cinfo);
}

GLOBAL(jpeg_cust_mem_data *)
jpeg_cust_mem_init(jpeg_cust_mem_data *custm, void *priv,
		j_custmem_init_ptr init,
		j_custmem_term_ptr term,
		j_custmem_avail_ptr avail,
		j_custmem_get_small_ptr get_small,
		j_custmem_free_small_ptr free_small,
		j_cust_mem_get_large_ptr get_large,
		j_custmem_free_large_ptr free_large,
		j_custmem_open_backing_store_ptr open_backing_store)
{
	jpeg_cust_mem_data *lcustm = NULL;

	/* We need at least the following for a viable memory manager */
	if (get_small && free_small && get_large && free_large)
	{
		lcustm = custm;

		lcustm->priv = priv;
		lcustm->j_mem_init = init;
		lcustm->j_mem_term = term;
		lcustm->j_mem_avail = avail;
		lcustm->j_mem_get_small = get_small;
		lcustm->j_mem_free_small = free_small;
		lcustm->j_mem_get_large = get_large;
		lcustm->j_mem_free_large = free_large;
		lcustm->j_mem_open_backing_store = open_backing_store;
	}
	return lcustm;
}

#endif /* !defined(SHARE_JPEG) || SHARE_JPEG==0 */
