/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005 Sun Microsystems, Inc.
 * Copyright (C)2009-2011 D. R. Commander
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
static const int pixelsize[NUMSUBOPT]={3, 3, 3, 1};

#define _throw(c) {sprintf(lasterror, "%s", c);  retval=-1;  goto bailout;}
#define checkhandle(h) jpgstruct *j=(jpgstruct *)h; \
	if(!j) {sprintf(lasterror, "Invalid handle");  return -1;}


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
	unsigned long retval=0;
	if(width<1 || height<1)
		_throw("Invalid argument in TJBUFSIZE()");

	// This allows for rare corner cases in which a JPEG image can actually be
	// larger than the uncompressed input (we wouldn't mention it if it hadn't
	// happened before.)
	retval=((width+15)&(~15)) * ((height+15)&(~15)) * 6 + 2048;

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL TJBUFSIZEYUV(int width, int height,
	int subsamp)
{
	unsigned long retval=0;
	int pw, ph, cw, ch;
	if(width<1 || height<1 || subsamp<0 || subsamp>=NUMSUBOPT)
		_throw("Invalid argument in TJBUFSIZEYUV()");
	pw=PAD(width, hsampfactor[subsamp]);
	ph=PAD(height, vsampfactor[subsamp]);
	cw=pw/hsampfactor[subsamp];  ch=ph/vsampfactor[subsamp];
	retval=PAD(pw, 4)*ph + (subsamp==TJ_GRAYSCALE? 0:PAD(cw, 4)*ch*2);

	bailout:
	return retval;
}


DLLEXPORT int DLLCALL tjCompress(tjhandle h,
	unsigned char *srcbuf, int width, int pitch, int height, int ps,
	unsigned char *dstbuf, unsigned long *size,
	int jpegsub, int qual, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
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
		retval=-1;
		goto bailout;
	}

	jpeg_set_defaults(&j->cinfo);

	jpeg_set_quality(&j->cinfo, qual, TRUE);
	if(jpegsub==TJ_GRAYSCALE)
		jpeg_set_colorspace(&j->cinfo, JCS_GRAYSCALE);
	else
		jpeg_set_colorspace(&j->cinfo, JCS_YCbCr);
	if(qual>=96) j->cinfo.dct_method=JDCT_ISLOW;
	else j->cinfo.dct_method=JDCT_FASTEST;

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
			_tmpbuf[i]=(JSAMPLE *)malloc(
				PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
					/compptr->h_samp_factor, 16) * cinfo->max_v_samp_factor + 16);
			if(!_tmpbuf[i]) _throw("Memory allocation failure");
			tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*cinfo->max_v_samp_factor);
			if(!tmpbuf[i]) _throw("Memory allocation failure");
			for(row=0; row<cinfo->max_v_samp_factor; row++)
			{
				unsigned char *_tmpbuf_aligned=
					(unsigned char *)PAD((size_t)_tmpbuf[i], 16);
				tmpbuf[i][row]=&_tmpbuf_aligned[
					PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
						/compptr->h_samp_factor, 16) * row];
			}
			_tmpbuf2[i]=(JSAMPLE *)malloc(PAD(compptr->width_in_blocks*DCTSIZE, 16)
				* compptr->v_samp_factor + 16);
			if(!_tmpbuf2[i]) _throw("Memory allocation failure");
			tmpbuf2[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*compptr->v_samp_factor);
			if(!tmpbuf2[i]) _throw("Memory allocation failure");
			for(row=0; row<compptr->v_samp_factor; row++)
			{
				unsigned char *_tmpbuf2_aligned=
					(unsigned char *)PAD((size_t)_tmpbuf2[i], 16);
				tmpbuf2[i][row]=&_tmpbuf2_aligned[
					PAD(compptr->width_in_blocks*DCTSIZE, 16) * row];
			}
			cw[i]=pw*compptr->h_samp_factor/cinfo->max_h_samp_factor;
			ch[i]=ph*compptr->v_samp_factor/cinfo->max_v_samp_factor;
			outbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ch[i]);
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
		jpeg_abort_compress(&j->cinfo);
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
		jpeg_finish_compress(&j->cinfo);
		*size=TJBUFSIZE(j->cinfo.image_width, j->cinfo.image_height)
			-(unsigned long)(j->jdms.free_in_buffer);
	}

	bailout:
	if(j->cinfo.global_state>CSTATE_START) jpeg_abort_compress(&j->cinfo);
	if(row_pointer) free(row_pointer);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]!=NULL) free(tmpbuf[i]);
		if(_tmpbuf[i]!=NULL) free(_tmpbuf[i]);
		if(tmpbuf2[i]!=NULL) free(tmpbuf2[i]);
		if(_tmpbuf2[i]!=NULL) free(_tmpbuf2[i]);
		if(outbuf[i]!=NULL) free(outbuf[i]);
	}
	return retval;
}


DLLEXPORT int DLLCALL tjEncodeYUV(tjhandle h,
	unsigned char *srcbuf, int width, int pitch, int height, int ps,
	unsigned char *dstbuf, int subsamp, int flags)
{
	unsigned long size;
	return tjCompress(h, srcbuf, width, pitch, height, ps, dstbuf, &size,
		subsamp, 0, flags|TJ_YUV);
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


DLLEXPORT int DLLCALL tjDecompressHeader2(tjhandle h,
	unsigned char *srcbuf, unsigned long size,
	int *width, int *height, int *jpegsub)
{
	int i, k, retval=0;

	checkhandle(h);

	if(srcbuf==NULL || size<=0 || width==NULL || height==NULL || jpegsub==NULL)
		_throw("Invalid argument in tjDecompressHeader2()");
	if(!j->initd) _throw("Instance has not been initialized for decompression");

	if(setjmp(j->jerr.jb))
	{  // this will execute if LIBJPEG has an error
		return -1;
	}

	j->jsms.bytes_in_buffer = size;
	j->jsms.next_input_byte = srcbuf;

	jpeg_read_header(&j->dinfo, TRUE);

	*width=j->dinfo.image_width;  *height=j->dinfo.image_height;
	*jpegsub=-1;
	for(i=0; i<NUMSUBOPT; i++)
	{
		if(j->dinfo.num_components==pixelsize[i])
		{
			if(j->dinfo.comp_info[0].h_samp_factor==hsampfactor[i]
				&& j->dinfo.comp_info[0].v_samp_factor==vsampfactor[i])
			{
				int match=0;
				for(k=1; k<j->dinfo.num_components; k++)
				{
					if(j->dinfo.comp_info[k].h_samp_factor==1
						&& j->dinfo.comp_info[k].v_samp_factor==1)
						match++;
				}
				if(match==j->dinfo.num_components-1)
				{
					*jpegsub=i;  break;
				}
			}
		}
	}

	jpeg_abort_decompress(&j->dinfo);

	if(*jpegsub<0) _throw("Could not determine subsampling type for JPEG image");
	if(*width<1 || *height<1) _throw("Invalid data returned in header");

	bailout:
	return retval;
}


DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle h,
	unsigned char *srcbuf, unsigned long size,
	int *width, int *height)
{
	int jpegsub;
	return tjDecompressHeader2(h, srcbuf, size, width, height, &jpegsub);
}


DLLEXPORT int DLLCALL tjDecompress(tjhandle h,
	unsigned char *srcbuf, unsigned long size,
	unsigned char *dstbuf, int width, int pitch, int height, int ps,
	int flags)
{
	int i, row, retval=0;  JSAMPROW *row_pointer=NULL, *outbuf[MAX_COMPONENTS];
	int cw[MAX_COMPONENTS], ch[MAX_COMPONENTS], iw[MAX_COMPONENTS],
		tmpbufsize=0, usetmpbuf=0, th[MAX_COMPONENTS];
	JSAMPLE *_tmpbuf=NULL;  JSAMPROW *tmpbuf[MAX_COMPONENTS];

	checkhandle(h);

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  outbuf[i]=NULL;
	}

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
		retval=-1;
		goto bailout;
	}

	j->jsms.bytes_in_buffer = size;
	j->jsms.next_input_byte = srcbuf;

	jpeg_read_header(&j->dinfo, TRUE);

	if(flags&TJ_YUV)
	{
		j_decompress_ptr dinfo=&j->dinfo;
		JSAMPLE *ptr=dstbuf;

		for(i=0; i<dinfo->num_components; i++)
		{
			jpeg_component_info *compptr=&dinfo->comp_info[i];
			int ih;
			iw[i]=compptr->width_in_blocks*DCTSIZE;
			ih=compptr->height_in_blocks*DCTSIZE;
			cw[i]=PAD(dinfo->image_width, dinfo->max_h_samp_factor)
				*compptr->h_samp_factor/dinfo->max_h_samp_factor;
			ch[i]=PAD(dinfo->image_height, dinfo->max_v_samp_factor)
				*compptr->v_samp_factor/dinfo->max_v_samp_factor;
			if(iw[i]!=cw[i] || ih!=ch[i]) usetmpbuf=1;
			th[i]=compptr->v_samp_factor*DCTSIZE;
			tmpbufsize+=iw[i]*th[i];
			if((outbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ch[i]))==NULL)
				_throw("Memory allocation failed in tjDecompress()");
			for(row=0; row<ch[i]; row++)
			{
				outbuf[i][row]=ptr;
				ptr+=PAD(cw[i], 4);
			}
		}
		if(usetmpbuf)
		{
			if((_tmpbuf=(JSAMPLE *)malloc(sizeof(JSAMPLE)*tmpbufsize))==NULL)
				_throw("Memory allocation failed in tjDecompress()");
			ptr=_tmpbuf;
			for(i=0; i<dinfo->num_components; i++)
			{
				if((tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*th[i]))==NULL)
					_throw("Memory allocation failed in tjDecompress()");
				for(row=0; row<th[i]; row++)
				{
					tmpbuf[i][row]=ptr;
					ptr+=iw[i];
				}
			}
		}
	}
	else
	{
		if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*height))==NULL)
			_throw("Memory allocation failed in tjDecompress()");
		for(i=0; i<height; i++)
		{
			if(flags&TJ_BOTTOMUP) row_pointer[i]= &dstbuf[(height-i-1)*pitch];
			else row_pointer[i]= &dstbuf[i*pitch];
		}
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
	if(flags&TJ_YUV) j->dinfo.raw_data_out=TRUE;

	jpeg_start_decompress(&j->dinfo);
	if(flags&TJ_YUV)
	{
		j_decompress_ptr dinfo=&j->dinfo;
		for(row=0; row<dinfo->output_height;
			row+=dinfo->max_v_samp_factor*DCTSIZE)
		{
			JSAMPARRAY yuvptr[MAX_COMPONENTS];
			int crow[MAX_COMPONENTS];
			for(i=0; i<dinfo->num_components; i++)
			{
				jpeg_component_info *compptr=&dinfo->comp_info[i];
				crow[i]=row*compptr->v_samp_factor/dinfo->max_v_samp_factor;
				if(usetmpbuf) yuvptr[i]=tmpbuf[i];
				else yuvptr[i]=&outbuf[i][crow[i]];
			}
			jpeg_read_raw_data(dinfo, yuvptr, dinfo->max_v_samp_factor*DCTSIZE);
			if(usetmpbuf)
			{
				int j;
				for(i=0; i<dinfo->num_components; i++)
				{
					for(j=0; j<min(th[i], ch[i]-crow[i]); j++)
					{
						memcpy(outbuf[i][crow[i]+j], tmpbuf[i][j], cw[i]);
					}
				}
			}
		}
	}
	else
	{
		while(j->dinfo.output_scanline<j->dinfo.output_height)
		{
			jpeg_read_scanlines(&j->dinfo, &row_pointer[j->dinfo.output_scanline],
				j->dinfo.output_height-j->dinfo.output_scanline);
		}
	}
	jpeg_finish_decompress(&j->dinfo);

	bailout:
	if(j->dinfo.global_state>DSTATE_START) jpeg_abort_decompress(&j->dinfo);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]) free(tmpbuf[i]);
		if(outbuf[i]) free(outbuf[i]);
	}
	if(_tmpbuf) free(_tmpbuf);
	if(row_pointer) free(row_pointer);
	return retval;
}


DLLEXPORT int DLLCALL tjDecompressToYUV(tjhandle h,
	unsigned char *srcbuf, unsigned long size,
	unsigned char *dstbuf, int flags)
{
	return tjDecompress(h, srcbuf, size, dstbuf, 1, 0, 1, 3, flags|TJ_YUV);
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
