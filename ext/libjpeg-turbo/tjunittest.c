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

/*
 * This program tests the various code paths in the TurboJPEG C Wrapper
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "./tjutil.h"
#include "./turbojpeg.h"
#ifdef _WIN32
 #include <time.h>
 #define random() rand()
#endif


void usage(char *progName)
{
	printf("\nUSAGE: %s [options]\n", progName);
	printf("Options:\n");
	printf("-yuv = test YUV encoding/decoding support\n");
	printf("-alloc = test automatic buffer allocation\n");
	exit(1);
}


#define _throwtj() {printf("TurboJPEG ERROR:\n%s\n", tjGetErrorStr());  \
	bailout();}
#define _tj(f) {if((f)==-1) _throwtj();}
#define _throw(m) {printf("ERROR: %s\n", m);  bailout();}

const char *subNameLong[TJ_NUMSAMP]=
{
	"4:4:4", "4:2:2", "4:2:0", "GRAY", "4:4:0"
};
const char *subName[TJ_NUMSAMP]={"444", "422", "420", "GRAY", "440"};

const char *pixFormatStr[TJ_NUMPF]=
{
	"RGB", "BGR", "RGBX", "BGRX", "XBGR", "XRGB", "Grayscale",
	"RGBA", "BGRA", "ABGR", "ARGB"
};

const int alphaOffset[TJ_NUMPF] = {-1, -1, -1, -1, -1, -1, -1, 3, 3, 0, 0};

const int _3byteFormats[]={TJPF_RGB, TJPF_BGR};
const int _4byteFormats[]={TJPF_RGBX, TJPF_BGRX, TJPF_XBGR, TJPF_XRGB};
const int _onlyGray[]={TJPF_GRAY};
const int _onlyRGB[]={TJPF_RGB};

enum {YUVENCODE=1, YUVDECODE};
int yuv=0, alloc=0;

int exitStatus=0;
#define bailout() {exitStatus=-1;  goto bailout;}


void initBuf(unsigned char *buf, int w, int h, int pf, int flags)
{
	int roffset=tjRedOffset[pf];
	int goffset=tjGreenOffset[pf];
	int boffset=tjBlueOffset[pf];
	int ps=tjPixelSize[pf];
	int index, row, col, halfway=16;

	memset(buf, 0, w*h*ps);
	if(pf==TJPF_GRAY)
	{
		for(row=0; row<h; row++)
		{
			for(col=0; col<w; col++)
			{
				if(flags&TJFLAG_BOTTOMUP) index=(h-row-1)*w+col;
				else index=row*w+col;
				if(((row/8)+(col/8))%2==0) buf[index]=(row<halfway)? 255:0;
				else buf[index]=(row<halfway)? 76:226;
			}
		}
	}
	else
	{
		for(row=0; row<h; row++)
		{
			for(col=0; col<w; col++)
			{
				if(flags&TJFLAG_BOTTOMUP) index=(h-row-1)*w+col;
				else index=row*w+col;
				if(((row/8)+(col/8))%2==0)
				{
					if(row<halfway)
					{
						buf[index*ps+roffset]=255;
						buf[index*ps+goffset]=255;
						buf[index*ps+boffset]=255;
					}
				}
				else
				{
					buf[index*ps+roffset]=255;
					if(row>=halfway) buf[index*ps+goffset]=255;
				}
			}
		}
	}
}


#define checkval(v, cv) { \
	if(v<cv-1 || v>cv+1) { \
		printf("\nComp. %s at %d,%d should be %d, not %d\n",  \
			#v, row, col, cv, v); \
		retval=0;  exitStatus=-1;  goto bailout; \
	}}

#define checkval0(v) { \
	if(v>1) { \
		printf("\nComp. %s at %d,%d should be 0, not %d\n", #v, row, col, v); \
		retval=0;  exitStatus=-1;  goto bailout; \
	}}

#define checkval255(v) { \
	if(v<254) { \
		printf("\nComp. %s at %d,%d should be 255, not %d\n", #v, row, col, v); \
		retval=0;  exitStatus=-1;  goto bailout; \
	}}


int checkBuf(unsigned char *buf, int w, int h, int pf, int subsamp,
	tjscalingfactor sf, int flags)
{
	int roffset=tjRedOffset[pf];
	int goffset=tjGreenOffset[pf];
	int boffset=tjBlueOffset[pf];
	int aoffset=alphaOffset[pf];
	int ps=tjPixelSize[pf];
	int index, row, col, retval=1;
	int halfway=16*sf.num/sf.denom;
	int blocksize=8*sf.num/sf.denom;

	for(row=0; row<h; row++)
	{
		for(col=0; col<w; col++)
		{
			unsigned char r, g, b, a;
			if(flags&TJFLAG_BOTTOMUP) index=(h-row-1)*w+col;
			else index=row*w+col;
			r=buf[index*ps+roffset];
			g=buf[index*ps+goffset];
			b=buf[index*ps+boffset];
			a=aoffset>=0? buf[index*ps+aoffset]:0xFF;
			if(((row/blocksize)+(col/blocksize))%2==0)
			{
				if(row<halfway)
				{
					checkval255(r);  checkval255(g);  checkval255(b);
				}
				else
				{
					checkval0(r);  checkval0(g);  checkval0(b);
				}
			}
			else
			{
				if(subsamp==TJSAMP_GRAY)
				{
					if(row<halfway)
					{
						checkval(r, 76);  checkval(g, 76);  checkval(b, 76);
					}
					else
					{
						checkval(r, 226);  checkval(g, 226);  checkval(b, 226);
					}
				}
				else
				{
					if(row<halfway)
					{
						checkval255(r);  checkval0(g);  checkval0(b);
					}
					else
					{
						checkval255(r);  checkval255(g);  checkval0(b);
					}
				}
			}
			checkval255(a);
		}
	}

	bailout:
	if(retval==0)
	{
		for(row=0; row<h; row++)
		{
			for(col=0; col<w; col++)
			{
				printf("%.3d/%.3d/%.3d ", buf[(row*w+col)*ps+roffset],
					buf[(row*w+col)*ps+goffset], buf[(row*w+col)*ps+boffset]);
			}
			printf("\n");
		}
	}
	return retval;
}


#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))

int checkBufYUV(unsigned char *buf, int w, int h, int subsamp)
{
	int row, col;
	int hsf=tjMCUWidth[subsamp]/8, vsf=tjMCUHeight[subsamp]/8;
	int pw=PAD(w, hsf), ph=PAD(h, vsf);
	int cw=pw/hsf, ch=ph/vsf;
	int ypitch=PAD(pw, 4), uvpitch=PAD(cw, 4);
	int retval=1;
	int halfway=16;

	for(row=0; row<ph; row++)
	{
		for(col=0; col<pw; col++)
		{
			unsigned char y=buf[ypitch*row+col];
			if(((row/8)+(col/8))%2==0)
			{
				if(row<halfway) checkval255(y)  else checkval0(y);
			}
			else
			{
				if(row<halfway) checkval(y, 76)  else checkval(y, 226);
			}
		}
	}
	if(subsamp!=TJSAMP_GRAY)
	{
		halfway=16/vsf;
		for(row=0; row<ch; row++)
		{
			for(col=0; col<cw; col++)
			{
				unsigned char u=buf[ypitch*ph + (uvpitch*row+col)],
					v=buf[ypitch*ph + uvpitch*ch + (uvpitch*row+col)];
				if(((row*vsf/8)+(col*hsf/8))%2==0)
				{
					checkval(u, 128);  checkval(v, 128);
				}
				else
				{
					if(row<halfway)
					{
						checkval(u, 85);  checkval255(v);
					}
					else
					{
						checkval0(u);  checkval(v, 149);
					}
				}
			}
		}
	}

	bailout:
	if(retval==0)
	{
		for(row=0; row<ph; row++)
		{
			for(col=0; col<pw; col++)
				printf("%.3d ", buf[ypitch*row+col]);
			printf("\n");
		}
		printf("\n");
		for(row=0; row<ch; row++)
		{
			for(col=0; col<cw; col++)
				printf("%.3d ", buf[ypitch*ph + (uvpitch*row+col)]);
			printf("\n");
		}
		printf("\n");
		for(row=0; row<ch; row++)
		{
			for(col=0; col<cw; col++)
				printf("%.3d ", buf[ypitch*ph + uvpitch*ch + (uvpitch*row+col)]);
			printf("\n");
		}
	}

	return retval;
}


void writeJPEG(unsigned char *jpegBuf, unsigned long jpegSize, char *filename)
{
	FILE *file=fopen(filename, "wb");
	if(!file || fwrite(jpegBuf, jpegSize, 1, file)!=1)
	{
		printf("ERROR: Could not write to %s.\n%s\n", filename, strerror(errno));
		bailout();
	}

	bailout:
	if(file) fclose(file);
}


void compTest(tjhandle handle, unsigned char **dstBuf,
	unsigned long *dstSize, int w, int h, int pf, char *basename,
	int subsamp, int jpegQual, int flags)
{
	char tempStr[1024];  unsigned char *srcBuf=NULL;
	double t;

	if(yuv==YUVENCODE)
		printf("%s %s -> %s YUV ... ", pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "Bottom-Up":"Top-Down ", subNameLong[subsamp]);
	else
		printf("%s %s -> %s Q%d ... ", pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "Bottom-Up":"Top-Down ", subNameLong[subsamp],
			jpegQual);

	if((srcBuf=(unsigned char *)malloc(w*h*tjPixelSize[pf]))==NULL)
		_throw("Memory allocation failure");
	initBuf(srcBuf, w, h, pf, flags);
	if(*dstBuf && *dstSize>0) memset(*dstBuf, 0, *dstSize);

	t=gettime();
	if(yuv==YUVENCODE)
	{
		_tj(tjEncodeYUV2(handle, srcBuf, w, 0, h, pf, *dstBuf, subsamp, flags));
	}
	else
	{
		if(!alloc)
		{
			flags|=TJFLAG_NOREALLOC;
			*dstSize=(yuv==YUVENCODE? tjBufSizeYUV(w, h, subsamp)
				: tjBufSize(w, h, subsamp));
		}
		_tj(tjCompress2(handle, srcBuf, w, 0, h, pf, dstBuf, dstSize, subsamp,
			jpegQual, flags));
	}
	t=gettime()-t;

	if(yuv==YUVENCODE)
		snprintf(tempStr, 1024, "%s_enc_%s_%s_%s.yuv", basename, pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "BU":"TD", subName[subsamp]);
	else
		snprintf(tempStr, 1024, "%s_enc_%s_%s_%s_Q%d.jpg", basename,
			pixFormatStr[pf], (flags&TJFLAG_BOTTOMUP)? "BU":"TD", subName[subsamp],
			jpegQual);
	writeJPEG(*dstBuf, *dstSize, tempStr);
	if(yuv==YUVENCODE)
	{
		if(checkBufYUV(*dstBuf, w, h, subsamp)) printf("Passed.");
		else printf("FAILED!");
	}
	else printf("Done.");
	printf("  %f ms\n  Result in %s\n", t*1000., tempStr);

	bailout:
	if(srcBuf) free(srcBuf);
}


void _decompTest(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, int w, int h, int pf, char *basename, int subsamp,
	int flags, tjscalingfactor sf)
{
	unsigned char *dstBuf=NULL;
	int _hdrw=0, _hdrh=0, _hdrsubsamp=-1;  double t;
	int scaledWidth=TJSCALED(w, sf);
	int scaledHeight=TJSCALED(h, sf);
	unsigned long dstSize=0;

	if(yuv==YUVENCODE) return;

	if(yuv==YUVDECODE)
		printf("JPEG -> YUV %s ... ", subNameLong[subsamp]);
	else
	{
		printf("JPEG -> %s %s ", pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "Bottom-Up":"Top-Down ");
		if(sf.num!=1 || sf.denom!=1)
			printf("%d/%d ... ", sf.num, sf.denom);
		else printf("... ");
	}

	_tj(tjDecompressHeader2(handle, jpegBuf, jpegSize, &_hdrw, &_hdrh,
		&_hdrsubsamp));
	if(_hdrw!=w || _hdrh!=h || _hdrsubsamp!=subsamp)
		_throw("Incorrect JPEG header");

	if(yuv==YUVDECODE) dstSize=tjBufSizeYUV(w, h, subsamp);
	else dstSize=scaledWidth*scaledHeight*tjPixelSize[pf];
	if((dstBuf=(unsigned char *)malloc(dstSize))==NULL)
		_throw("Memory allocation failure");
	memset(dstBuf, 0, dstSize);

	t=gettime();
	if(yuv==YUVDECODE)
	{
		_tj(tjDecompressToYUV(handle, jpegBuf, jpegSize, dstBuf, flags));
	}
	else
	{
		_tj(tjDecompress2(handle, jpegBuf, jpegSize, dstBuf, scaledWidth, 0,
			scaledHeight, pf, flags));
	}
	t=gettime()-t;

	if(yuv==YUVDECODE)
	{
		if(checkBufYUV(dstBuf, w, h, subsamp)) printf("Passed.");
		else printf("FAILED!");
	}
	else
	{
		if(checkBuf(dstBuf, scaledWidth, scaledHeight, pf, subsamp, sf, flags))
			printf("Passed.");
		else printf("FAILED!");
	}
	printf("  %f ms\n", t*1000.);

	bailout:
	if(dstBuf) free(dstBuf);
}


void decompTest(tjhandle handle, unsigned char *jpegBuf,
	unsigned long jpegSize, int w, int h, int pf, char *basename, int subsamp,
	int flags)
{
	int i, n=0;
	tjscalingfactor *sf=tjGetScalingFactors(&n), sf1={1, 1};
	if(!sf || !n) _throwtj();

	if((subsamp==TJSAMP_444 || subsamp==TJSAMP_GRAY) && !yuv)
	{
		for(i=0; i<n; i++)
			_decompTest(handle, jpegBuf, jpegSize, w, h, pf, basename, subsamp,
				flags, sf[i]);
	}
	else
		_decompTest(handle, jpegBuf, jpegSize, w, h, pf, basename, subsamp, flags,
			sf1);

	bailout:
	return;
}


void doTest(int w, int h, const int *formats, int nformats, int subsamp,
	char *basename)
{
	tjhandle chandle=NULL, dhandle=NULL;
	unsigned char *dstBuf=NULL;
	unsigned long size=0;  int pfi, pf, i;

	if(!alloc)
	{
		size=(yuv==YUVENCODE? tjBufSizeYUV(w, h, subsamp)
			: tjBufSize(w, h, subsamp));
		if((dstBuf=(unsigned char *)tjAlloc(size))==NULL)
			_throw("Memory allocation failure.");
	}

	if((chandle=tjInitCompress())==NULL || (dhandle=tjInitDecompress())==NULL)
		_throwtj();

	for(pfi=0; pfi<nformats; pfi++)
	{
		for(i=0; i<2; i++)
		{
			int flags=0;
			if(subsamp==TJSAMP_422 || subsamp==TJSAMP_420 || subsamp==TJSAMP_440)
				flags|=TJFLAG_FASTUPSAMPLE;
			if(i==1)
			{
				if(yuv==YUVDECODE) goto bailout;
				else flags|=TJFLAG_BOTTOMUP;
			}
			pf=formats[pfi];
			compTest(chandle, &dstBuf, &size, w, h, pf, basename, subsamp, 100,
				flags);
			decompTest(dhandle, dstBuf, size, w, h, pf, basename, subsamp,
				flags);
			if(pf>=TJPF_RGBX && pf<=TJPF_XRGB)
				decompTest(dhandle, dstBuf, size, w, h, pf+(TJPF_RGBA-TJPF_RGBX),
					basename, subsamp, flags);
			printf("\n");
		}
	}
	printf("--------------------\n\n");

	bailout:
	if(chandle) tjDestroy(chandle);
	if(dhandle) tjDestroy(dhandle);

	if(dstBuf) tjFree(dstBuf);
}


void bufSizeTest(void)
{
	int w, h, i, subsamp;
	unsigned char *srcBuf=NULL, *jpegBuf=NULL;
	tjhandle handle=NULL;
	unsigned long jpegSize=0;

	if((handle=tjInitCompress())==NULL) _throwtj();

	printf("Buffer size regression test\n");
	for(subsamp=0; subsamp<TJ_NUMSAMP; subsamp++)
	{
		for(w=1; w<48; w++)
		{
			int maxh=(w==1)? 2048:48;
			for(h=1; h<maxh; h++)
			{
				if(h%100==0) printf("%.4d x %.4d\b\b\b\b\b\b\b\b\b\b\b", w, h);
				if((srcBuf=(unsigned char *)malloc(w*h*4))==NULL)
					_throw("Memory allocation failure");
				if(!alloc)
				{
					if((jpegBuf=(unsigned char *)tjAlloc(tjBufSize(w, h, subsamp)))
						==NULL)
						_throw("Memory allocation failure");
					jpegSize=tjBufSize(w, h, subsamp);
				}

				for(i=0; i<w*h*4; i++)
				{
					if(random()<RAND_MAX/2) srcBuf[i]=0;
					else srcBuf[i]=255;
				}

				_tj(tjCompress2(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBuf,
					&jpegSize, subsamp, 100, alloc? 0:TJFLAG_NOREALLOC));
				free(srcBuf);  srcBuf=NULL;
				tjFree(jpegBuf);  jpegBuf=NULL;

				if((srcBuf=(unsigned char *)malloc(h*w*4))==NULL)
					_throw("Memory allocation failure");
				if(!alloc)
				{
					if((jpegBuf=(unsigned char *)tjAlloc(tjBufSize(h, w, subsamp)))
						==NULL)
						_throw("Memory allocation failure");
					jpegSize=tjBufSize(h, w, subsamp);
				}

				for(i=0; i<h*w*4; i++)
				{
					if(random()<RAND_MAX/2) srcBuf[i]=0;
					else srcBuf[i]=255;
				}

				_tj(tjCompress2(handle, srcBuf, h, 0, w, TJPF_BGRX, &jpegBuf,
					&jpegSize, subsamp, 100, alloc? 0:TJFLAG_NOREALLOC));
				free(srcBuf);  srcBuf=NULL;
				tjFree(jpegBuf);  jpegBuf=NULL;
			}
		}
	}
	printf("Done.      \n");

	bailout:
	if(srcBuf) free(srcBuf);
	if(jpegBuf) free(jpegBuf);
	if(handle) tjDestroy(handle);
}


int main(int argc, char *argv[])
{
	int doyuv=0, i;
	#ifdef _WIN32
	srand((unsigned int)time(NULL));
	#endif
	if(argc>1)
	{
		for(i=1; i<argc; i++)
		{
			if(!strcasecmp(argv[i], "-yuv")) doyuv=1;
			if(!strcasecmp(argv[i], "-alloc")) alloc=1;
			if(!strncasecmp(argv[i], "-h", 2) || !strcasecmp(argv[i], "-?"))
				usage(argv[0]);
		}
	}
	if(alloc) printf("Testing automatic buffer allocation\n");
	if(doyuv) {yuv=YUVENCODE;  alloc=0;}
	doTest(35, 39, _3byteFormats, 2, TJSAMP_444, "test");
	doTest(39, 41, _4byteFormats, 4, TJSAMP_444, "test");
	doTest(41, 35, _3byteFormats, 2, TJSAMP_422, "test");
	doTest(35, 39, _4byteFormats, 4, TJSAMP_422, "test");
	doTest(39, 41, _3byteFormats, 2, TJSAMP_420, "test");
	doTest(41, 35, _4byteFormats, 4, TJSAMP_420, "test");
	doTest(35, 39, _3byteFormats, 2, TJSAMP_440, "test");
	doTest(39, 41, _4byteFormats, 4, TJSAMP_440, "test");
	doTest(35, 39, _onlyGray, 1, TJSAMP_GRAY, "test");
	doTest(39, 41, _3byteFormats, 2, TJSAMP_GRAY, "test");
	doTest(41, 35, _4byteFormats, 4, TJSAMP_GRAY, "test");
	if(!doyuv) bufSizeTest();
	if(doyuv)
	{
		yuv=YUVDECODE;
		doTest(48, 48, _onlyRGB, 1, TJSAMP_444, "test_yuv0");
		doTest(35, 39, _onlyRGB, 1, TJSAMP_444, "test_yuv1");
		doTest(48, 48, _onlyRGB, 1, TJSAMP_422, "test_yuv0");
		doTest(39, 41, _onlyRGB, 1, TJSAMP_422, "test_yuv1");
		doTest(48, 48, _onlyRGB, 1, TJSAMP_420, "test_yuv0");
		doTest(41, 35, _onlyRGB, 1, TJSAMP_420, "test_yuv1");
		doTest(48, 48, _onlyRGB, 1, TJSAMP_440, "test_yuv0");
		doTest(35, 39, _onlyRGB, 1, TJSAMP_440, "test_yuv1");
		doTest(48, 48, _onlyRGB, 1, TJSAMP_GRAY, "test_yuv0");
		doTest(35, 39, _onlyRGB, 1, TJSAMP_GRAY, "test_yuv1");
		doTest(48, 48, _onlyGray, 1, TJSAMP_GRAY, "test_yuv0");
		doTest(39, 41, _onlyGray, 1, TJSAMP_GRAY, "test_yuv1");
	}

	return exitStatus;
}
