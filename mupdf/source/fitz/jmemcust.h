/* Copyright (C) 2001-2017 Artifex Software, Inc.
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

typedef JMETHOD(long, j_custmem_init_ptr, (j_common_ptr cinfo));
typedef JMETHOD(void, j_custmem_term_ptr, (j_common_ptr cinfo));
typedef JMETHOD(long, j_custmem_avail_ptr, (j_common_ptr cinfo));
typedef JMETHOD(void *, j_custmem_get_small_ptr, (j_common_ptr cinfo, size_t size));
typedef JMETHOD(void, j_custmem_free_small_ptr, (j_common_ptr cinfo, void *object, size_t size));
typedef JMETHOD(void *, j_cust_mem_get_large_ptr, (j_common_ptr cinfo, size_t size));
typedef JMETHOD(void, j_custmem_free_large_ptr, (j_common_ptr cinfo, void *object, size_t size));
typedef JMETHOD(void, j_custmem_open_backing_store_ptr, (j_common_ptr cinfo, backing_store_ptr info, long total_bytes_needed));

typedef struct {
	j_custmem_init_ptr j_mem_init;
	j_custmem_term_ptr j_mem_term;
	j_custmem_avail_ptr j_mem_avail;
	j_custmem_get_small_ptr j_mem_get_small;
	j_custmem_free_small_ptr j_mem_free_small;
	j_cust_mem_get_large_ptr j_mem_get_large;
	j_custmem_free_large_ptr j_mem_free_large;
	j_custmem_open_backing_store_ptr j_mem_open_backing_store;
	void *priv;
} jpeg_cust_mem_data;

#define GET_CUST_MEM_DATA(c) ((jpeg_cust_mem_data *)c->client_data)

GLOBAL(jpeg_cust_mem_data *)
	jpeg_cust_mem_init(jpeg_cust_mem_data *custm, void *priv,
			j_custmem_init_ptr init,
			j_custmem_term_ptr term,
			j_custmem_avail_ptr avail,
			j_custmem_get_small_ptr get_small,
			j_custmem_free_small_ptr free_small,
			j_cust_mem_get_large_ptr get_large,
			j_custmem_free_large_ptr free_large,
			j_custmem_open_backing_store_ptr open_backing_store);
