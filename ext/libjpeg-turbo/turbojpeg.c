/*
 * Copyright (C)2009-2012 D. R. Commander.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* TurboJPEG/OSS:  this implements the TurboJPEG API using libjpeg-turbo */

#include <stdio.h>
#include <stdlib.h>
#include <jinclude.h>
#define JPEG_INTERNALS
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include "./turbojpeg.h"
#include "./tjutil.h"
#include "transupp.h"

extern void jpeg_mem_dest_tj(j_compress_ptr, unsigned char **,
	unsigned long *, boolean);
extern void jpeg_mem_src_tj(j_decompress_ptr, unsigned char *, unsigned long);

#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))


/* Error handling (based on example in example.c) */

static char errStr[JMSG_LENGTH_MAX]="No error";

struct my_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};
typedef struct my_error_mgr *my_error_ptr;

static void my_error_exit(j_common_ptr cinfo)
{
	my_error_ptr myerr=(my_error_ptr)cinfo->err;
	(*cinfo->err->output_message)(cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

/* Based on output_message() in jerror.c */

static void my_output_message(j_common_ptr cinfo)
{
	(*cinfo->err->format_message)(cinfo, errStr);
}


/* Global structures, macros, etc. */

enum {COMPRESS=1, DECOMPRESS=2};

typedef struct _tjinstance
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_decompress_struct dinfo;
	struct my_error_mgr jerr;
	int init;
} tjinstance;

static const int pixelsize[TJ_NUMSAMP]={3, 3, 3, 1, 3};

static const JXFORM_CODE xformtypes[TJ_NUMXOP]=
{
	JXFORM_NONE, JXFORM_FLIP_H, JXFORM_FLIP_V, JXFORM_TRANSPOSE,
	JXFORM_TRANSVERSE, JXFORM_ROT_90, JXFORM_ROT_180, JXFORM_ROT_270
};

#define NUMSF 4
static const tjscalingfactor sf[NUMSF]={
	{1, 1},
	{1, 2},
	{1, 4},
	{1, 8}
};

#define _throw(m) {snprintf(errStr, JMSG_LENGTH_MAX, "%s", m);  \
	retval=-1;  goto bailout;}
#define getinstance(handle) tjinstance *this=(tjinstance *)handle;  \
	j_compress_ptr cinfo=NULL;  j_decompress_ptr dinfo=NULL;  \
	if(!this) {snprintf(errStr, JMSG_LENGTH_MAX, "Invalid handle");  \
		return -1;}  \
	cinfo=&this->cinfo;  dinfo=&this->dinfo;

static int getPixelFormat(int pixelSize, int flags)
{
	if(pixelSize==1) return TJPF_GRAY;
	if(pixelSize==3)
	{
		if(flags&TJ_BGR) return TJPF_BGR;
		else return TJPF_RGB;
	}
	if(pixelSize==4)
	{
		if(flags&TJ_ALPHAFIRST)
		{
			if(flags&TJ_BGR) return TJPF_XBGR;
			else return TJPF_XRGB;
		}
		else
		{
			if(flags&TJ_BGR) return TJPF_BGRX;
			else return TJPF_RGBX;
		}
	}
	return -1;
}

static int setCompDefaults(struct jpeg_compress_struct *cinfo,
	int pixelFormat, int subsamp, int jpegQual, int flags)
{
	int retval=0;

	switch(pixelFormat)
	{
		case TJPF_GRAY:
			cinfo->in_color_space=JCS_GRAYSCALE;  break;
		#if JCS_EXTENSIONS==1
		case TJPF_RGB:
			cinfo->in_color_space=JCS_EXT_RGB;  break;
		case TJPF_BGR:
			cinfo->in_color_space=JCS_EXT_BGR;  break;
		case TJPF_RGBX:
		case TJPF_RGBA:
			cinfo->in_color_space=JCS_EXT_RGBX;  break;
		case TJPF_BGRX:
		case TJPF_BGRA:
			cinfo->in_color_space=JCS_EXT_BGRX;  break;
		case TJPF_XRGB:
		case TJPF_ARGB:
			cinfo->in_color_space=JCS_EXT_XRGB;  break;
		case TJPF_XBGR:
		case TJPF_ABGR:
			cinfo->in_color_space=JCS_EXT_XBGR;  break;
		#else
		case TJPF_RGB:
			if(RGB_RED==0 && RGB_GREEN==1 && RGB_BLUE==2 && RGB_PIXELSIZE==3)
			{
				cinfo->in_color_space=JCS_RGB;  break;
			}
		default:
			_throw("Unsupported pixel format");
		#endif
	}

	cinfo->input_components=tjPixelSize[pixelFormat];
	jpeg_set_defaults(cinfo);
	if(jpegQual>=0)
	{
		jpeg_set_quality(cinfo, jpegQual, TRUE);
		if(jpegQual>=96 || flags&TJFLAG_ACCURATEDCT) cinfo->dct_method=JDCT_ISLOW;
		else cinfo->dct_method=JDCT_FASTEST;
	}
	if(subsamp==TJSAMP_GRAY)
		jpeg_set_colorspace(cinfo, JCS_GRAYSCALE);
	else
		jpeg_set_colorspace(cinfo, JCS_YCbCr);

	cinfo->comp_info[0].h_samp_factor=tjMCUWidth[subsamp]/8;
	cinfo->comp_info[1].h_samp_factor=1;
	cinfo->comp_info[2].h_samp_factor=1;
	cinfo->comp_info[0].v_samp_factor=tjMCUHeight[subsamp]/8;
	cinfo->comp_info[1].v_samp_factor=1;
	cinfo->comp_info[2].v_samp_factor=1;

	#if JCS_EXTENSIONS!=1
	bailout:
	#endif
	return retval;
}

static int setDecompDefaults(struct jpeg_decompress_struct *dinfo,
	int pixelFormat, int flags)
{
	int retval=0;

	switch(pixelFormat)
	{
		case TJPF_GRAY:
			dinfo->out_color_space=JCS_GRAYSCALE;  break;
		#if JCS_EXTENSIONS==1
		case TJPF_RGB:
			dinfo->out_color_space=JCS_EXT_RGB;  break;
		case TJPF_BGR:
			dinfo->out_color_space=JCS_EXT_BGR;  break;
		case TJPF_RGBX:
			dinfo->out_color_space=JCS_EXT_RGBX;  break;
		case TJPF_BGRX:
			dinfo->out_color_space=JCS_EXT_BGRX;  break;
		case TJPF_XRGB:
			dinfo->out_color_space=JCS_EXT_XRGB;  break;
		case TJPF_XBGR:
			dinfo->out_color_space=JCS_EXT_XBGR;  break;
		#if JCS_ALPHA_EXTENSIONS==1
		case TJPF_RGBA:
			dinfo->out_color_space=JCS_EXT_RGBA;  break;
		case TJPF_BGRA:
			dinfo->out_color_space=JCS_EXT_BGRA;  break;
		case TJPF_ARGB:
			dinfo->out_color_space=JCS_EXT_ARGB;  break;
		case TJPF_ABGR:
			dinfo->out_color_space=JCS_EXT_ABGR;  break;
		#endif
		#else
		case TJPF_RGB:
			if(RGB_RED==0 && RGB_GREEN==1 && RGB_BLUE==2 && RGB_PIXELSIZE==3)
			{
				dinfo->out_color_space=JCS_RGB;  break;
			}
		#endif
		default:
			_throw("Unsupported pixel format");
	}

	if(flags&TJFLAG_FASTDCT) dinfo->dct_method=JDCT_FASTEST;

	bailout:
	return retval;
}


static int getSubsamp(j_decompress_ptr dinfo)
{
	int retval=-1, i, k;
	for(i=0; i<NUMSUBOPT; i++)
	{
		if(dinfo->num_components==pixelsize[i])
		{
			if(dinfo->comp_info[0].h_samp_factor==tjMCUWidth[i]/8
				&& dinfo->comp_info[0].v_samp_factor==tjMCUHeight[i]/8)
			{
				int match=0;
				for(k=1; k<dinfo->num_components; k++)
				{
					if(dinfo->comp_info[k].h_samp_factor==1
						&& dinfo->comp_info[k].v_samp_factor==1)
						match++;
				}
				if(match==dinfo->num_components-1)
				{
					retval=i;  break;
				}
			}
		}
	}
	return retval;
}


/* General API functions */

DLLEXPORT char* DLLCALL tjGetErrorStr(void)
{
	return errStr;
}


DLLEXPORT int DLLCALL tjDestroy(tjhandle handle)
{
	getinstance(handle);
	if(setjmp(this->jerr.setjmp_buffer)) return -1;
	if(this->init&COMPRESS) jpeg_destroy_compress(cinfo);
	if(this->init&DECOMPRESS) jpeg_destroy_decompress(dinfo);
	free(this);
	return 0;
}


/* These are exposed mainly because Windows can't malloc() and free() across
   DLL boundaries except when the CRT DLL is used, and we don't use the CRT DLL
   with turbojpeg.dll for compatibility reasons.  However, these functions
   can potentially be used for other purposes by different implementations. */

DLLEXPORT void DLLCALL tjFree(unsigned char *buf)
{
	if(buf) free(buf);
}


DLLEXPORT unsigned char *DLLCALL tjAlloc(int bytes)
{
	return (unsigned char *)malloc(bytes);
}


/* Compressor  */

static tjhandle _tjInitCompress(tjinstance *this)
{
	unsigned char buffer[1], *buf=buffer;  unsigned long size=1;

	/* This is also straight out of example.c */
	this->cinfo.err=jpeg_std_error(&this->jerr.pub);
	this->jerr.pub.error_exit=my_error_exit;
	this->jerr.pub.output_message=my_output_message;

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		if(this) free(this);  return NULL;
	}

	jpeg_create_compress(&this->cinfo);
	/* Make an initial call so it will create the destination manager */
	jpeg_mem_dest_tj(&this->cinfo, &buf, &size, 0);

	this->init|=COMPRESS;
	return (tjhandle)this;
}

DLLEXPORT tjhandle DLLCALL tjInitCompress(void)
{
	tjinstance *this=NULL;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitCompress(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	return _tjInitCompress(this);
}


DLLEXPORT unsigned long DLLCALL tjBufSize(int width, int height,
	int jpegSubsamp)
{
	unsigned long retval=0;  int mcuw, mcuh, chromasf;
	if(width<1 || height<1 || jpegSubsamp<0 || jpegSubsamp>=NUMSUBOPT)
		_throw("tjBufSize(): Invalid argument");

	// This allows for rare corner cases in which a JPEG image can actually be
	// larger than the uncompressed input (we wouldn't mention it if it hadn't
	// happened before.)
	mcuw=tjMCUWidth[jpegSubsamp];
	mcuh=tjMCUHeight[jpegSubsamp];
	chromasf=jpegSubsamp==TJSAMP_GRAY? 0: 4*64/(mcuw*mcuh);
	retval=PAD(width, mcuw) * PAD(height, mcuh) * (2 + chromasf) + 2048;

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL TJBUFSIZE(int width, int height)
{
	unsigned long retval=0;
	if(width<1 || height<1)
		_throw("TJBUFSIZE(): Invalid argument");

	// This allows for rare corner cases in which a JPEG image can actually be
	// larger than the uncompressed input (we wouldn't mention it if it hadn't
	// happened before.)
	retval=PAD(width, 16) * PAD(height, 16) * 6 + 2048;

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL tjBufSizeYUV(int width, int height,
	int subsamp)
{
	unsigned long retval=0;
	int pw, ph, cw, ch;
	if(width<1 || height<1 || subsamp<0 || subsamp>=NUMSUBOPT)
		_throw("tjBufSizeYUV(): Invalid argument");
	pw=PAD(width, tjMCUWidth[subsamp]/8);
	ph=PAD(height, tjMCUHeight[subsamp]/8);
	cw=pw*8/tjMCUWidth[subsamp];  ch=ph*8/tjMCUHeight[subsamp];
	retval=PAD(pw, 4)*ph + (subsamp==TJSAMP_GRAY? 0:PAD(cw, 4)*ch*2);

	bailout:
	return retval;
}


DLLEXPORT unsigned long DLLCALL TJBUFSIZEYUV(int width, int height,
	int subsamp)
{
	return tjBufSizeYUV(width, height, subsamp);
}


DLLEXPORT int DLLCALL tjCompress2(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char **jpegBuf,
	unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags)
{
	int i, retval=0, alloc=1;  JSAMPROW *row_pointer=NULL;

	getinstance(handle)
	if((this->init&COMPRESS)==0)
		_throw("tjCompress2(): Instance has not been initialized for compression");

	if(srcBuf==NULL || width<=0 || pitch<0 || height<=0 || pixelFormat<0
		|| pixelFormat>=TJ_NUMPF || jpegBuf==NULL || jpegSize==NULL
		|| jpegSubsamp<0 || jpegSubsamp>=NUMSUBOPT || jpegQual<0 || jpegQual>100)
		_throw("tjCompress2(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(pitch==0) pitch=width*tjPixelSize[pixelFormat];

	cinfo->image_width=width;
	cinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(flags&TJFLAG_NOREALLOC)
	{
		alloc=0;  *jpegSize=tjBufSize(width, height, jpegSubsamp);
	}
	jpeg_mem_dest_tj(cinfo, jpegBuf, jpegSize, alloc);
	if(setCompDefaults(cinfo, pixelFormat, jpegSubsamp, jpegQual, flags)==-1)
		return -1;

	jpeg_start_compress(cinfo, TRUE);
	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*height))==NULL)
		_throw("tjCompress2(): Memory allocation failure");
	for(i=0; i<height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP) row_pointer[i]=&srcBuf[(height-i-1)*pitch];
		else row_pointer[i]=&srcBuf[i*pitch];
	}
	while(cinfo->next_scanline<cinfo->image_height)
	{
		jpeg_write_scanlines(cinfo, &row_pointer[cinfo->next_scanline],
			cinfo->image_height-cinfo->next_scanline);
	}
	jpeg_finish_compress(cinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	if(row_pointer) free(row_pointer);
	return retval;
}

DLLEXPORT int DLLCALL tjCompress(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelSize, unsigned char *jpegBuf,
	unsigned long *jpegSize, int jpegSubsamp, int jpegQual, int flags)
{
	int retval=0;  unsigned long size;
	if(flags&TJ_YUV)
	{
		size=tjBufSizeYUV(width, height, jpegSubsamp);
		retval=tjEncodeYUV2(handle, srcBuf, width, pitch, height,
			getPixelFormat(pixelSize, flags), jpegBuf, jpegSubsamp, flags);
	}
	else
	{
		retval=tjCompress2(handle, srcBuf, width, pitch, height,
			getPixelFormat(pixelSize, flags), &jpegBuf, &size, jpegSubsamp, jpegQual,
			flags|TJFLAG_NOREALLOC);
	}
	*jpegSize=size;
	return retval;
}


DLLEXPORT int DLLCALL tjEncodeYUV2(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelFormat, unsigned char *dstBuf,
	int subsamp, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	JSAMPLE *_tmpbuf[MAX_COMPONENTS], *_tmpbuf2[MAX_COMPONENTS];
	JSAMPROW *tmpbuf[MAX_COMPONENTS], *tmpbuf2[MAX_COMPONENTS];
	JSAMPROW *outbuf[MAX_COMPONENTS];
	int row, pw, ph, cw[MAX_COMPONENTS], ch[MAX_COMPONENTS];
	JSAMPLE *ptr=dstBuf;
	unsigned long yuvsize=0;
	jpeg_component_info *compptr;

	getinstance(handle);
	if((this->init&COMPRESS)==0)
		_throw("tjEncodeYUV2(): Instance has not been initialized for compression");

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  _tmpbuf[i]=NULL;
		tmpbuf2[i]=NULL;  _tmpbuf2[i]=NULL;  outbuf[i]=NULL;
	}

	if(srcBuf==NULL || width<=0 || pitch<0 || height<=0 || pixelFormat<0
		|| pixelFormat>=TJ_NUMPF || dstBuf==NULL || subsamp<0
		|| subsamp>=NUMSUBOPT)
		_throw("tjEncodeYUV2(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	if(pitch==0) pitch=width*tjPixelSize[pixelFormat];

	cinfo->image_width=width;
	cinfo->image_height=height;

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	yuvsize=tjBufSizeYUV(width, height, subsamp);
	jpeg_mem_dest_tj(cinfo, &dstBuf, &yuvsize, 0);
	if(setCompDefaults(cinfo, pixelFormat, subsamp, -1, flags)==-1) return -1;

	jpeg_start_compress(cinfo, TRUE);
	pw=PAD(width, cinfo->max_h_samp_factor);
	ph=PAD(height, cinfo->max_v_samp_factor);

	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)*ph))==NULL)
		_throw("tjEncodeYUV2(): Memory allocation failure");
	for(i=0; i<height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP) row_pointer[i]=&srcBuf[(height-i-1)*pitch];
		else row_pointer[i]=&srcBuf[i*pitch];
	}
	if(height<ph)
		for(i=height; i<ph; i++) row_pointer[i]=row_pointer[height-1];

	for(i=0; i<cinfo->num_components; i++)
	{
		compptr=&cinfo->comp_info[i];
		_tmpbuf[i]=(JSAMPLE *)malloc(
			PAD((compptr->width_in_blocks*cinfo->max_h_samp_factor*DCTSIZE)
				/compptr->h_samp_factor, 16) * cinfo->max_v_samp_factor + 16);
		if(!_tmpbuf[i]) _throw("tjEncodeYUV2(): Memory allocation failure");
		tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*cinfo->max_v_samp_factor);
		if(!tmpbuf[i]) _throw("tjEncodeYUV2(): Memory allocation failure");
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
		if(!_tmpbuf2[i]) _throw("tjEncodeYUV2(): Memory allocation failure");
		tmpbuf2[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*compptr->v_samp_factor);
		if(!tmpbuf2[i]) _throw("tjEncodeYUV2(): Memory allocation failure");
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
		if(!outbuf[i]) _throw("tjEncodeYUV2(): Memory allocation failure");
		for(row=0; row<ch[i]; row++)
		{
			outbuf[i][row]=ptr;
			ptr+=PAD(cw[i], 4);
		}
	}
	if(yuvsize!=(unsigned long)(ptr-dstBuf))
		_throw("tjEncodeYUV2(): Generated image is not the correct size");

	for(row=0; row<ph; row+=cinfo->max_v_samp_factor)
	{
		(*cinfo->cconvert->color_convert)(cinfo, &row_pointer[row], tmpbuf, 0,
			cinfo->max_v_samp_factor);
		(cinfo->downsample->downsample)(cinfo, tmpbuf, 0, tmpbuf2, 0);
		for(i=0, compptr=cinfo->comp_info; i<cinfo->num_components; i++, compptr++)
			jcopy_sample_rows(tmpbuf2[i], 0, outbuf[i],
				row*compptr->v_samp_factor/cinfo->max_v_samp_factor,
				compptr->v_samp_factor, cw[i]);
	}
	cinfo->next_scanline+=height;
	jpeg_abort_compress(cinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
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

DLLEXPORT int DLLCALL tjEncodeYUV(tjhandle handle, unsigned char *srcBuf,
	int width, int pitch, int height, int pixelSize, unsigned char *dstBuf,
	int subsamp, int flags)
{
	return tjEncodeYUV2(handle, srcBuf, width, pitch, height,
		getPixelFormat(pixelSize, flags), dstBuf, subsamp, flags);
}


/* Decompressor */

static tjhandle _tjInitDecompress(tjinstance *this)
{
	unsigned char buffer[1];

	/* This is also straight out of example.c */
	this->dinfo.err=jpeg_std_error(&this->jerr.pub);
	this->jerr.pub.error_exit=my_error_exit;
	this->jerr.pub.output_message=my_output_message;

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		if(this) free(this);  return NULL;
	}

	jpeg_create_decompress(&this->dinfo);
	/* Make an initial call so it will create the source manager */
	jpeg_mem_src_tj(&this->dinfo, buffer, 1);

	this->init|=DECOMPRESS;
	return (tjhandle)this;
}

DLLEXPORT tjhandle DLLCALL tjInitDecompress(void)
{
	tjinstance *this;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitDecompress(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	return _tjInitDecompress(this);
}


DLLEXPORT int DLLCALL tjDecompressHeader2(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height,
	int *jpegSubsamp)
{
	int retval=0;

	getinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompressHeader2(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || width==NULL || height==NULL
		|| jpegSubsamp==NULL)
		_throw("tjDecompressHeader2(): Invalid argument");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		return -1;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
	jpeg_read_header(dinfo, TRUE);

	*width=dinfo->image_width;
	*height=dinfo->image_height;
	*jpegSubsamp=getSubsamp(dinfo);

	jpeg_abort_decompress(dinfo);

	if(*jpegSubsamp<0)
		_throw("tjDecompressHeader2(): Could not determine subsampling type for JPEG image");
	if(*width<1 || *height<1)
		_throw("tjDecompressHeader2(): Invalid data returned in header");

	bailout:
	return retval;
}

DLLEXPORT int DLLCALL tjDecompressHeader(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, int *width, int *height)
{
	int jpegSubsamp;
	return tjDecompressHeader2(handle, jpegBuf, jpegSize, width, height,
		&jpegSubsamp);
}


DLLEXPORT tjscalingfactor* DLLCALL tjGetScalingFactors(int *numscalingfactors)
{
	if(numscalingfactors==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjGetScalingFactors(): Invalid argument");
		return NULL;
	}

	*numscalingfactors=NUMSF;
	return (tjscalingfactor *)sf;
}


DLLEXPORT int DLLCALL tjDecompress2(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelFormat, int flags)
{
	int i, retval=0;  JSAMPROW *row_pointer=NULL;
	int jpegwidth, jpegheight, scaledw, scaledh;

	getinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompress2(): Instance has not been initialized for decompression");

	if(jpegBuf==NULL || jpegSize<=0 || dstBuf==NULL || width<0 || pitch<0
		|| height<0 || pixelFormat<0 || pixelFormat>=TJ_NUMPF)
		_throw("tjDecompress2(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
	jpeg_read_header(dinfo, TRUE);
	if(setDecompDefaults(dinfo, pixelFormat, flags)==-1)
	{
		retval=-1;  goto bailout;
	}

	if(flags&TJFLAG_FASTUPSAMPLE) dinfo->do_fancy_upsampling=FALSE;

	jpegwidth=dinfo->image_width;  jpegheight=dinfo->image_height;
	if(width==0) width=jpegwidth;
	if(height==0) height=jpegheight;
	for(i=0; i<NUMSF; i++)
	{
		scaledw=TJSCALED(jpegwidth, sf[i]);
		scaledh=TJSCALED(jpegheight, sf[i]);
		if(scaledw<=width && scaledh<=height)
				break;
	}
	if(scaledw>width || scaledh>height)
		_throw("tjDecompress2(): Could not scale down to desired image dimensions");
	width=scaledw;  height=scaledh;
	dinfo->scale_num=sf[i].num;
	dinfo->scale_denom=sf[i].denom;

	jpeg_start_decompress(dinfo);
	if(pitch==0) pitch=dinfo->output_width*tjPixelSize[pixelFormat];
	if((row_pointer=(JSAMPROW *)malloc(sizeof(JSAMPROW)
		*dinfo->output_height))==NULL)
		_throw("tjDecompress2(): Memory allocation failure");
	for(i=0; i<(int)dinfo->output_height; i++)
	{
		if(flags&TJFLAG_BOTTOMUP)
			row_pointer[i]=&dstBuf[(dinfo->output_height-i-1)*pitch];
		else row_pointer[i]=&dstBuf[i*pitch];
	}
	while(dinfo->output_scanline<dinfo->output_height)
	{
		jpeg_read_scanlines(dinfo, &row_pointer[dinfo->output_scanline],
			dinfo->output_height-dinfo->output_scanline);
	}
	jpeg_finish_decompress(dinfo);

	bailout:
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	if(row_pointer) free(row_pointer);
	return retval;
}

DLLEXPORT int DLLCALL tjDecompress(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, unsigned char *dstBuf, int width, int pitch,
	int height, int pixelSize, int flags)
{
	if(flags&TJ_YUV)
		return tjDecompressToYUV(handle, jpegBuf, jpegSize, dstBuf, flags);
	else
		return tjDecompress2(handle, jpegBuf, jpegSize, dstBuf, width, pitch,
			height, getPixelFormat(pixelSize, flags), flags);
}


DLLEXPORT int DLLCALL tjDecompressToYUV(tjhandle handle,
	unsigned char *jpegBuf, unsigned long jpegSize, unsigned char *dstBuf,
	int flags)
{
	int i, row, retval=0;  JSAMPROW *outbuf[MAX_COMPONENTS];
	int cw[MAX_COMPONENTS], ch[MAX_COMPONENTS], iw[MAX_COMPONENTS],
		tmpbufsize=0, usetmpbuf=0, th[MAX_COMPONENTS];
	JSAMPLE *_tmpbuf=NULL, *ptr=dstBuf;  JSAMPROW *tmpbuf[MAX_COMPONENTS];

	getinstance(handle);
	if((this->init&DECOMPRESS)==0)
		_throw("tjDecompressToYUV(): Instance has not been initialized for decompression");

	for(i=0; i<MAX_COMPONENTS; i++)
	{
		tmpbuf[i]=NULL;  outbuf[i]=NULL;
	}

	if(jpegBuf==NULL || jpegSize<=0 || dstBuf==NULL)
		_throw("tjDecompressToYUV(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);
	jpeg_read_header(dinfo, TRUE);

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
			_throw("tjDecompressToYUV(): Memory allocation failure");
		for(row=0; row<ch[i]; row++)
		{
			outbuf[i][row]=ptr;
			ptr+=PAD(cw[i], 4);
		}
	}
	if(usetmpbuf)
	{
		if((_tmpbuf=(JSAMPLE *)malloc(sizeof(JSAMPLE)*tmpbufsize))==NULL)
			_throw("tjDecompressToYUV(): Memory allocation failure");
		ptr=_tmpbuf;
		for(i=0; i<dinfo->num_components; i++)
		{
			if((tmpbuf[i]=(JSAMPROW *)malloc(sizeof(JSAMPROW)*th[i]))==NULL)
				_throw("tjDecompressToYUV(): Memory allocation failure");
			for(row=0; row<th[i]; row++)
			{
				tmpbuf[i][row]=ptr;
				ptr+=iw[i];
			}
		}
	}

	if(flags&TJFLAG_FASTUPSAMPLE) dinfo->do_fancy_upsampling=FALSE;
	dinfo->raw_data_out=TRUE;

	jpeg_start_decompress(dinfo);
	for(row=0; row<(int)dinfo->output_height;
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
	jpeg_finish_decompress(dinfo);

	bailout:
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	for(i=0; i<MAX_COMPONENTS; i++)
	{
		if(tmpbuf[i]) free(tmpbuf[i]);
		if(outbuf[i]) free(outbuf[i]);
	}
	if(_tmpbuf) free(_tmpbuf);
	return retval;
}


/* Transformer */

DLLEXPORT tjhandle DLLCALL tjInitTransform(void)
{
	tjinstance *this=NULL;  tjhandle handle=NULL;
	if((this=(tjinstance *)malloc(sizeof(tjinstance)))==NULL)
	{
		snprintf(errStr, JMSG_LENGTH_MAX,
			"tjInitTransform(): Memory allocation failure");
		return NULL;
	}
	MEMZERO(this, sizeof(tjinstance));
	handle=_tjInitCompress(this);
	if(!handle) return NULL;
	handle=_tjInitDecompress(this);
	return handle;
}


DLLEXPORT int DLLCALL tjTransform(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, int n, unsigned char **dstBufs,
	unsigned long *dstSizes, tjtransform *t, int flags)
{
	jpeg_transform_info *xinfo=NULL;
	jvirt_barray_ptr *srccoefs, *dstcoefs;
	int retval=0, i, jpegSubsamp;

	getinstance(handle);
	if((this->init&COMPRESS)==0 || (this->init&DECOMPRESS)==0)
		_throw("tjTransform(): Instance has not been initialized for transformation");

	if(jpegBuf==NULL || jpegSize<=0 || n<1 || dstBufs==NULL || dstSizes==NULL
		|| t==NULL || flags<0)
		_throw("tjTransform(): Invalid argument");

	if(flags&TJFLAG_FORCEMMX) putenv("JSIMD_FORCEMMX=1");
	else if(flags&TJFLAG_FORCESSE) putenv("JSIMD_FORCESSE=1");
	else if(flags&TJFLAG_FORCESSE2) putenv("JSIMD_FORCESSE2=1");

	if(setjmp(this->jerr.setjmp_buffer))
	{
		/* If we get here, the JPEG code has signaled an error. */
		retval=-1;
		goto bailout;
	}

	jpeg_mem_src_tj(dinfo, jpegBuf, jpegSize);

	if((xinfo=(jpeg_transform_info *)malloc(sizeof(jpeg_transform_info)*n))
		==NULL)
		_throw("tjTransform(): Memory allocation failure");
	MEMZERO(xinfo, sizeof(jpeg_transform_info)*n);

	for(i=0; i<n; i++)
	{
		xinfo[i].transform=xformtypes[t[i].op];
		xinfo[i].perfect=(t[i].options&TJXOPT_PERFECT)? 1:0;
		xinfo[i].trim=(t[i].options&TJXOPT_TRIM)? 1:0;
		xinfo[i].force_grayscale=(t[i].options&TJXOPT_GRAY)? 1:0;
		xinfo[i].crop=(t[i].options&TJXOPT_CROP)? 1:0;
		if(n!=1 && t[i].op==TJXOP_HFLIP) xinfo[i].slow_hflip=1;
		else xinfo[i].slow_hflip=0;

		if(xinfo[i].crop)
		{
			xinfo[i].crop_xoffset=t[i].r.x;  xinfo[i].crop_xoffset_set=JCROP_POS;
			xinfo[i].crop_yoffset=t[i].r.y;  xinfo[i].crop_yoffset_set=JCROP_POS;
			if(t[i].r.w!=0)
			{
				xinfo[i].crop_width=t[i].r.w;  xinfo[i].crop_width_set=JCROP_POS;
			}
			else xinfo[i].crop_width=JCROP_UNSET;
			if(t[i].r.h!=0)
			{
				xinfo[i].crop_height=t[i].r.h;  xinfo[i].crop_height_set=JCROP_POS;
			}
			else xinfo[i].crop_height=JCROP_UNSET;
		}
	}

	jcopy_markers_setup(dinfo, JCOPYOPT_ALL);
	jpeg_read_header(dinfo, TRUE);
	jpegSubsamp=getSubsamp(dinfo);
	if(jpegSubsamp<0)
		_throw("tjTransform(): Could not determine subsampling type for JPEG image");

	for(i=0; i<n; i++)
	{
		if(!jtransform_request_workspace(dinfo, &xinfo[i]))
			_throw("tjTransform(): Transform is not perfect");

		if(xinfo[i].crop)
		{
			if((t[i].r.x%xinfo[i].iMCU_sample_width)!=0
				|| (t[i].r.y%xinfo[i].iMCU_sample_height)!=0)
			{
				snprintf(errStr, JMSG_LENGTH_MAX,
					"To crop this JPEG image, x must be a multiple of %d\n"
					"and y must be a multiple of %d.\n",
					xinfo[i].iMCU_sample_width, xinfo[i].iMCU_sample_height);
				retval=-1;  goto bailout;
			}
		}
	}

	srccoefs=jpeg_read_coefficients(dinfo);

	for(i=0; i<n; i++)
	{
		int w, h, alloc=1;
		if(!xinfo[i].crop)
		{
			w=dinfo->image_width;  h=dinfo->image_height;
		}
		else
		{
			w=xinfo[i].crop_width;  h=xinfo[i].crop_height;
		}
		if(flags&TJFLAG_NOREALLOC)
		{
			alloc=0;  dstSizes[i]=tjBufSize(w, h, jpegSubsamp);
		}
		if(!(t[i].options&TJXOPT_NOOUTPUT))
			jpeg_mem_dest_tj(cinfo, &dstBufs[i], &dstSizes[i], alloc);
		jpeg_copy_critical_parameters(dinfo, cinfo);
		dstcoefs=jtransform_adjust_parameters(dinfo, cinfo, srccoefs,
			&xinfo[i]);
		if(!(t[i].options&TJXOPT_NOOUTPUT))
		{
			jpeg_write_coefficients(cinfo, dstcoefs);
			jcopy_markers_execute(dinfo, cinfo, JCOPYOPT_ALL);
		}
		else jinit_c_master_control(cinfo, TRUE);
		jtransform_execute_transformation(dinfo, cinfo, srccoefs,
			&xinfo[i]);
		if(t[i].customFilter)
		{
			int ci, y;  JDIMENSION by;
			for(ci=0; ci<cinfo->num_components; ci++)
			{
				jpeg_component_info *compptr=&cinfo->comp_info[ci];
				tjregion arrayRegion={0, 0, compptr->width_in_blocks*DCTSIZE,
					DCTSIZE};
				tjregion planeRegion={0, 0, compptr->width_in_blocks*DCTSIZE,
					compptr->height_in_blocks*DCTSIZE};
				for(by=0; by<compptr->height_in_blocks; by+=compptr->v_samp_factor)
				{
					JBLOCKARRAY barray=(dinfo->mem->access_virt_barray)
						((j_common_ptr)dinfo, dstcoefs[ci], by, compptr->v_samp_factor,
						TRUE);
					for(y=0; y<compptr->v_samp_factor; y++)
					{
						if(t[i].customFilter(barray[y][0], arrayRegion, planeRegion,
							ci, i, &t[i])==-1)
							_throw("tjTransform(): Error in custom filter");
						arrayRegion.y+=DCTSIZE;
					}
				}
			}
		}
		if(!(t[i].options&TJXOPT_NOOUTPUT)) jpeg_finish_compress(cinfo);
	}

	jpeg_finish_decompress(dinfo);

	bailout:
	if(cinfo->global_state>CSTATE_START) jpeg_abort_compress(cinfo);
	if(dinfo->global_state>DSTATE_START) jpeg_abort_decompress(dinfo);
	if(xinfo) free(xinfo);
	return retval;
}
