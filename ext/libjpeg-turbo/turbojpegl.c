/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005 Sun Microsystems, Inc.
 * Copyright (C)2009-2010 D. R. Commander
 *
 * This library is free software and may be redistributed and/or modified under
 * the terms of the wxWindows Library License, Version 3.1 or (at your option)
 * any later version.  The full license is in the LICENSE.txt file included
 * with this distribution.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * wxWindows Library License for more details.
 */

// This implements a JPEG compressor/decompressor using the libjpeg API

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define JPEG_INTERNALS
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include "./turbojpeg.h"
#ifdef sun
#include <malloc.h>
#endif

void *__memalign(size_t boundary, size_t size)
{
	#if defined(_WIN32) || defined(__APPLE__)
	return malloc(size);
	#else
	#ifdef sun
	return memalign(boundary, size);
	#else
	void *ptr=NULL;
	posix_memalign(&ptr, boundary, size);
	return ptr;
	#endif
	#endif
}

#ifndef min
 #define min(a,b) ((a)<(b)?(a):(b))
#endif

#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))


// Error handling

static char lasterror[JMSG_LENGTH_MAX]="No error";

typedef struct _error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf jb;
} error_mgr;

static void my_error_exit(j_common_ptr cinfo)
{
	error_mgr *myerr = (error_mgr *)cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp(myerr->jb, 1);
}

static void my_output_message(j_common_ptr cinfo)
{
	(*cinfo->err->format_message)(cinfo, lasterror);
}


// Global structures, macros, etc.

typedef struct _jpgstruct
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_decompress_struct dinfo;
	struct jpeg_destination_mgr jdms;
	struct jpeg_source_mgr jsms;
	error_mgr jerr;
	int initc, initd;
} jpgstruct;

static const int hsampfactor[NUMSUBOPT]={1, 2, 2, 1};
static const int vsampfactor[NUMSUBOPT]={1, 1, 2, 1};

#define _throw(c) {sprintf(lasterror, "%s", c);  return -1;}
#define _catch(f) {if((f)==-1) return -1;}
#define checkhandle(h) jpgstruct *j=(jpgstruct *)h; \
	if(!j) _throw("Invalid handle");


// CO

static boolean empty_output_buffer(struct jpeg_compress_struct *cinfo)
{
	ERREXIT(cinfo, JERR_BUFFER_SIZE);
	return TRUE;
}

static void destination_noop(struct jpeg_compress_struct *cinfo)
{
}

DLLEXPORT tjhandle DLLCALL tjInitCompress(void)
{
	jpgstruct *j=NULL;
	if((j=(jpgstruct *)malloc(sizeof(jpgstruct)))==NULL)
		{sprintf(lasterror, "Memory allocation failure");  return NULL;}
	memset(j, 0, sizeof(jpgstruct));
	j->cinfo.err=jpeg_std_error(&j->jerr.pub);
	j->jerr.pub.error_exit=my_error_exit;
	j->jerr.pub.output_message=my_output_message;

	if(setjmp(j->jerr.jb))
	{ // this will execute if LIBJPEG has an error
		if(j) free(j);  return NULL;
	}

	jpeg_create_compress(&j->cinfo);
	j->cinfo.dest=&j->jdms;
	j->jdms.init_destination=destination_noop;
	j->jdms.empty_output_buffer=empty_output_buffer;
	j->jdms.term_destination=destination_noop;

	j->initc=1;
	return (tjhandle)j;
}

DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height)
{
	// This allows enough room in case the image doesn't compress
	return ((width+15)&(~15)) * ((height+15)&(~15)) * 6 + 2048;
}

DLLEXPORT int DLLCALL tjCompress(tjhandle h,
	unsigned char *srcbuf, int width, int pitch, int height, int ps,
	unsigned char *dstbuf, unsigned long *size,
	int jpegsub, int qual, int flags)
{
	int i;  JSAMPROW *row_pointer=NULL;
	JSAMPLE *_tmpbuf[MAX_COMPONENTS], *_tmpbuf2[MAX_COMPONENTS];
	JSAMPROW *tmpbuf[MAX_COMPONENTS], *tmpbuf2[MAX_COMPONENTS];
	JSAMPROW *outbuf[MAX_COMPONENTS];

	checkhandle(h);

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  _tmpbuf[i]=NULL;
		tmpbuf2[i]=NULL;  _tmpbuf2[i]=NULL;  outbuf[i]=NULL;
	}

	if(srcbuf==NULL || width<=0 || pitch<0 || height<=0
		|| dstbuf==NULL || size==NULL
		|| jpegsub<0 || jpegsub>=NUMSUBOPT || qual<0 || qual>100)
		_throw("Invalid argument in tjCompress()");
	if(ps!=3 && ps!=4 && ps!=1)
		_throw("This compressor can only handle 24-bit and 32-bit RGB or 8-bit grayscale input");
	if(!j->initc) _throw("Instance has not been initialized for compression");

	if(pitch==0) pitch=width*ps;

	j->cinfo.image_width = width;
	j->cinfo.image_height = height;
	j->cinfo.input_components = ps;

	if(ps==1) j->cinfo.in_color_space = JCS_GRAYSCALE;
	#if JCS_EXTENSIONS==1
	else j->cinfo.in_color_space = JCS_EXT_RGB;
	if(ps==3 && (flags&TJ_BGR))
		j->cinfo.in_color_space = JCS_EXT_BGR;
	else if(ps==4 && !(flags&TJ_BGR) && !(flags&TJ_ALPHAFIRST))
		j->cinfo.in_color_space = JCS_EXT_RGBX;
	else if(ps==4 && (flags&TJ_BGR) && !(flags&TJ_ALPHAFIRST))
		j->cinfo.in_color_space = JCS_EXT_BGRX;
	else if(ps==4 && (flags&TJ_BGR) && (flags&TJ_ALPHAFIRST))
		j->cinfo.in_color_space = JCS_EXT_XBGR;
	else if(ps==4 && !(flags&TJ_BGR) && (flags&TJ_ALPHAFIRST))
		j->cinfo.in_color_space = JCS_EXT_XRGB;
	#else
	#error "TurboJPEG requires JPEG colorspace extensions"
	#endif

	if(flags&TJ_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJ_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJ_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(j->jerr.jb))
	{  // this will execute if LIBJPEG has an error
		if(row_pointer) free(row_pointer);
		for(i=0; i<MAX_COMPONENTS; i++)
		{
			if(tmpbuf[i]!=NULL) free(tmpbuf[i]);
			if(_tmpbuf[i]!=NULL) free(_tmpbuf[i]);
			if(tmpbuf2[i]!=NULL) free(tmpbuf2[i]);
			if(_tmpbuf2[i]!=NULL) free(_tmpbuf2[i]);
			if(outbuf[i]!=NULL) free(outbuf[i]);
		}
		return -1;
	}

	jpeg_set_defaults(&j->cinfo);

	jpeg_set_quality(&j->cinfo, qual, TRUE);
	if(jpegsub==TJ_GRAYSCALE)
		jpeg_set_colorspace(&j->cinfo, JCS_GRAYSCALE);
	else
		jpeg_set_colorspace(&j->cinfo, JCS_YCbCr);
	j->cinfo.dct_method = JDCT_FASTEST;

	j->cinfo.comp_info[0].h_samp_factor=hsampfactor[jpegsub];
	j->cinfo.comp_info[1].h_samp_factor=1;
	j->cinfo.comp_info[2].h_samp_factor=1;
	j->cinfo.comp_info[0].v_samp_factor=vsampfactor[jpegsub];
	j->cinfo.comp_info[1].v_samp_factor=1;
	j->cinfo.comp_info[2].v_samp_factor=1;

	j->jdms.next_output_byte = dstbuf;
	j->jdms.free_in_buffer = TJBUFSIZE(j->cinfo.image_width, j->cinfo.image_height);

	jpeg_start_compress(&j->cinfo, TRUE);
	if(flags&TJ_YUV)
	{
		j_compress_ptr cinfo=&j->cinfo;
		int row;
		int pw=PAD(width, cinfo->max_h_samp_factor);
		int ph=PAD(height, cinfo->max_v_samp_factor);
		int cw[MAX_COMPONENTS], ch[MAX_COMPONENTS];
		jpeg_component_info *compptr;
		JSAMPLE *ptr=dstbuf;  unsigned long yuvsize=0;

		if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph))==NULL)
			_throw("Memory allocation failed in tjCompress()");
		for(i=0; i<height; i++)
		{
			if(flags&TJ_BOTTOMUP) row_pointer[i]= &srcbuf[(height-i-1)*pitch];
			else row_pointer[i]= &srcbuf[i*pitch];
		}
		if(height<ph)
			for(i=height; i<ph; i++) row_pointer[i]=row_pointer[height-1];

		for(i=0; i<cinfo->num_components; i++)
		{
			compptr=&cinfo->comp_info[i];
			_tmpbuf[i]=(JSAMPLE *)__memalign(16,
				PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
					/compptr->h_samp_factor, 16) * cinfo->max_v_samp_factor);
			if(!_tmpbuf[i]) _throw("Memory allocation failure");
			tmpbuf[i]=(JSAMPROW *)__memalign(16,
				sizeof(JSAMPROW)*cinfo->max_v_samp_factor);
			if(!tmpbuf[i]) _throw("Memory allocation failure");
			for(row=0; row<cinfo->max_v_samp_factor; row++)
				tmpbuf[i][row]=&_tmpbuf[i][
					PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
						/compptr->h_samp_factor, 16) * row];
			_tmpbuf2[i]=(JSAMPLE *)__memalign(16,
				PAD(compptr->width_in_blocks*DCTSIZE, 16) * compptr->v_samp_factor);
			if(!_tmpbuf2[i]) _throw("Memory allocation failure");
			tmpbuf2[i]=(JSAMPROW *)__memalign(16,
				sizeof(JSAMPROW)*compptr->v_samp_factor);
			if(!tmpbuf2[i]) _throw("Memory allocation failure");
			for(row=0; row<compptr->v_samp_factor; row++)
				tmpbuf2[i][row]=&_tmpbuf2[i][
					PAD(compptr->width_in_blocks*DCTSIZE, 16) * row];
			cw[i]=pw*compptr->h_samp_factor/cinfo->max_h_samp_factor;
			ch[i]=ph*compptr->v_samp_factor/cinfo->max_v_samp_factor;
			outbuf[i]=(JSAMPROW *)__memalign(16, sizeof(JSAMPROW)*ch[i]);
			if(!outbuf[i]) _throw("Memory allocation failure");
			for(row=0; row<ch[i]; row++)
			{
				outbuf[i][row]=ptr;
				ptr+=PAD(cw[i], 4);
			}
		}
		yuvsize=(unsigned long)(ptr-dstbuf);

		for(row=0; row<ph; row+=cinfo->max_v_samp_factor)
		{
			(*cinfo->cconvert->color_convert)(cinfo, &row_pointer[row], tmpbuf,
				0, cinfo->max_v_samp_factor);
			(cinfo->downsample->downsample)(cinfo, tmpbuf, 0, tmpbuf2, 0);
			for(i=0, compptr=cinfo->comp_info; i<cinfo->num_components;
				i++, compptr++)
				jcopy_sample_rows(tmpbuf2[i], 0, outbuf[i],
					row*compptr->v_samp_factor/cinfo->max_v_samp_factor,
					compptr->v_samp_factor, cw[i]);
		}
		*size=yuvsize;
		cinfo->next_scanline+=height;
	}
	else
	{
		if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*height))==NULL)
			_throw("Memory allocation failed in tjCompress()");
		for(i=0; i<height; i++)
		{
			if(flags&TJ_BOTTOMUP) row_pointer[i]= &srcbuf[(height-i-1)*pitch];
			else row_pointer[i]= &srcbuf[i*pitch];
		}
		while(j->cinfo.next_scanline<j->cinfo.image_height)
		{
			jpeg_write_scanlines(&j->cinfo, &row_pointer[j->cinfo.next_scanline],
				j->cinfo.image_height-j->cinfo.next_scanline);
		}
	}
	jpeg_finish_compress(&j->cinfo);
	if(!(flags&TJ_YUV))
		*size=TJBUFSIZE(j->cinfo.image_width, j->cinfo.image_height)
			-(unsigned long)(j->jdms.free_in_buffer);

	if(row_pointer) free(row_pointer);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]!=NULL) free(tmpbuf[i]);
		if(_tmpbuf[i]!=NULL) free(_tmpbuf[i]);
		if(tmpbuf2[i]!=NULL) free(tmpbuf2[i]);
		if(_tmpbuf2[i]!=NULL) free(_tmpbuf2[i]);
		if(outbuf[i]!=NULL) free(outbuf[i]);
	}
	return 0;
}


// DEC

static boolean fill_input_buffer (struct jpeg_decompress_struct *dinfo)
{
	ERREXIT(dinfo, JERR_BUFFER_SIZE);
	return TRUE;
}

static void skip_input_data (struct jpeg_decompress_struct *dinfo, long num_bytes)
{
	dinfo->src->next_input_byte += (size_t) num_bytes;
	dinfo->src->bytes_in_buffer -= (size_t) num_bytes;
}

static void source_noop (struct jpeg_decompress_struct *dinfo)
{
}

DLLEXPORT tjhandle DLLCALL tjInitDecompress(void)
{
	jpgstruct *j;
	if((j=(jpgstruct *)malloc(sizeof(jpgstruct)))==NULL)
		{sprintf(lasterror, "Memory allocation failure");  return NULL;}
	memset(j, 0, sizeof(jpgstruct));
	j->dinfo.err=jpeg_std_error(&j->jerr.pub);
	j->jerr.pub.error_exit=my_error_exit;
	j->jerr.pub.output_message=my_output_message;

	if(setjmp(j->jerr.jb))
	{ // this will execute if LIBJPEG has an error
		free(j);  return NULL;
  }

	jpeg_create_decompress(&j->dinfo);
	j->dinfo.src=&j->jsms;
	j->jsms.init_source=source_noop;
	j->jsms.fill_input_buffer = fill_input_buffer;
	j->jsms.skip_input_data = skip_input_data;
	j->jsms.resync_to_restart = jpeg_resync_to_restart;
	j->jsms.term_source = source_noop;

	j->initd=1;
	return (tjhandle)j;
}


DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle h,
	unsigned char *srcbuf, unsigned long size,
	int *width, int *height)
{
	checkhandle(h);

	if(srcbuf==NULL || size<=0 || width==NULL || height==NULL)
		_throw("Invalid argument in tjDecompressHeader()");
	if(!j->initd) _throw("Instance has not been initialized for decompression");

	if(setjmp(j->jerr.jb))
	{  // this will execute if LIBJPEG has an error
		return -1;
	}

	j->jsms.bytes_in_buffer = size;
	j->jsms.next_input_byte = srcbuf;

	jpeg_read_header(&j->dinfo, TRUE);

	*width=j->dinfo.image_width;  *height=j->dinfo.image_height;

	jpeg_abort_decompress(&j->dinfo);

	if(*width<1 || *height<1) _throw("Invalid data returned in header");
	return 0;
}


DLLEXPORT int DLLCALL tjDecompress(tjhandle h,
	unsigned char *srcbuf, unsigned long size,
	unsigned char *dstbuf, int width, int pitch, int height, int ps,
	int flags)
{
	int i;  JSAMPROW *row_pointer=NULL;

	checkhandle(h);

	if(srcbuf==NULL || size<=0
		|| dstbuf==NULL || width<=0 || pitch<0 || height<=0)
		_throw("Invalid argument in tjDecompress()");
	if(ps!=3 && ps!=4 && ps!=1)
		_throw("This decompressor can only handle 24-bit and 32-bit RGB or 8-bit grayscale output");
	if(!j->initd) _throw("Instance has not been initialized for decompression");

	if(pitch==0) pitch=width*ps;

	if(flags&TJ_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJ_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJ_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(j->jerr.jb))
	{  // this will execute if LIBJPEG has an error
		if(row_pointer) free(row_pointer);
		return -1;
  }

	j->jsms.bytes_in_buffer = size;
	j->jsms.next_input_byte = srcbuf;

	jpeg_read_header(&j->dinfo, TRUE);

	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*height))==NULL)
		_throw("Memory allocation failed in tjInitDecompress()");
	for(i=0; i<height; i++)
	{
		if(flags&TJ_BOTTOMUP) row_pointer[i]= &dstbuf[(height-i-1)*pitch];
		else row_pointer[i]= &dstbuf[i*pitch];
	}

	if(ps==1) j->dinfo.out_color_space = JCS_GRAYSCALE;
	#if JCS_EXTENSIONS==1
	else j->dinfo.out_color_space = JCS_EXT_RGB;
	if(ps==3 && (flags&TJ_BGR))
		j->dinfo.out_color_space = JCS_EXT_BGR;
	else if(ps==4 && !(flags&TJ_BGR) && !(flags&TJ_ALPHAFIRST))
		j->dinfo.out_color_space = JCS_EXT_RGBX;
	else if(ps==4 && (flags&TJ_BGR) && !(flags&TJ_ALPHAFIRST))
		j->dinfo.out_color_space = JCS_EXT_BGRX;
	else if(ps==4 && (flags&TJ_BGR) && (flags&TJ_ALPHAFIRST))
		j->dinfo.out_color_space = JCS_EXT_XBGR;
	else if(ps==4 && !(flags&TJ_BGR) && (flags&TJ_ALPHAFIRST))
		j->dinfo.out_color_space = JCS_EXT_XRGB;
	#else
	#error "TurboJPEG requires JPEG colorspace extensions"
	#endif

	if(flags&TJ_FASTUPSAMPLE) j->dinfo.do_fancy_upsampling=FALSE;

	jpeg_start_decompress(&j->dinfo);
	while(j->dinfo.output_scanline<j->dinfo.output_height)
	{
		jpeg_read_scanlines(&j->dinfo, &row_pointer[j->dinfo.output_scanline],
			j->dinfo.output_height-j->dinfo.output_scanline);
	}
	jpeg_finish_decompress(&j->dinfo);

	if(row_pointer) free(row_pointer);
	return 0;
}


// General

DLLEXPORT char* DLLCALL tjGetErrorStr(void)
{
	return lasterror;
}

DLLEXPORT int DLLCALL tjDestroy(tjhandle h)
{
	checkhandle(h);
	if(setjmp(j->jerr.jb)) return -1;
	if(j->initc) jpeg_destroy_compress(&j->cinfo);
	if(j->initd) jpeg_destroy_decompress(&j->dinfo);
	free(j);
	return 0;
}
