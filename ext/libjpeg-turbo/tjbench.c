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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <cdjpeg.h>
#include "./bmp.h"
#include "./tjutil.h"
#include "./turbojpeg.h"


#define _throw(op, err) {  \
	printf("ERROR in line %d while %s:\n%s\n", __LINE__, op, err);  \
  retval=-1;  goto bailout;}
#define _throwunix(m) _throw(m, strerror(errno))
#define _throwtj(m) _throw(m, tjGetErrorStr())
#define _throwbmp(m) _throw(m, bmpgeterr())

enum {YUVENCODE=1, YUVDECODE};
int flags=TJFLAG_NOREALLOC, decomponly=0, yuv=0, quiet=0, dotile=0,
	pf=TJPF_BGR;
char *ext="ppm";
const char *pixFormatStr[TJ_NUMPF]=
{
	"RGB", "BGR", "RGBX", "BGRX", "XBGR", "XRGB", "GRAY"
};
const char *subNameLong[TJ_NUMSAMP]=
{
	"4:4:4", "4:2:2", "4:2:0", "GRAY", "4:4:0"
};
const char *subName[NUMSUBOPT]={"444", "422", "420", "GRAY", "440"};
tjscalingfactor *scalingfactors=NULL, sf={1, 1};  int nsf=0;
int xformop=TJXOP_NONE, xformopt=0;
int (*customFilter)(short *, tjregion, tjregion, int, int, tjtransform *);
double benchtime=5.0;


char *sigfig(double val, int figs, char *buf, int len)
{
	char format[80];
	int digitsafterdecimal=figs-(int)ceil(log10(fabs(val)));
	if(digitsafterdecimal<1) snprintf(format, 80, "%%.0f");
	else snprintf(format, 80, "%%.%df", digitsafterdecimal);
	snprintf(buf, len, format, val);
	return buf;
}


/* Custom DCT filter which produces a negative of the image */
int dummyDCTFilter(short *coeffs, tjregion arrayRegion, tjregion planeRegion,
	int componentIndex, int transformIndex, tjtransform *transform)
{
	int i;
	for(i=0; i<arrayRegion.w*arrayRegion.h; i++) coeffs[i]=-coeffs[i];
	return 0;
}


/* Decompression test */
int decomptest(unsigned char *srcbuf, unsigned char **jpegbuf,
	unsigned long *jpegsize, unsigned char *dstbuf, int w, int h,
	int subsamp, int jpegqual, char *filename, int tilew, int tileh)
{
	char tempstr[1024], sizestr[20]="\0", qualstr[6]="\0", *ptr;
	FILE *file=NULL;  tjhandle handle=NULL;
	int row, col, i, dstbufalloc=0, retval=0;
	double start, elapsed;
	int ps=tjPixelSize[pf];
	int yuvsize=tjBufSizeYUV(w, h, subsamp), bufsize;
	int scaledw=(yuv==YUVDECODE)? w : TJSCALED(w, sf);
	int scaledh=(yuv==YUVDECODE)? h : TJSCALED(h, sf);
	int pitch=scaledw*ps;
	int ntilesw=(w+tilew-1)/tilew, ntilesh=(h+tileh-1)/tileh;
	unsigned char *dstptr, *dstptr2;

	if(jpegqual>0)
	{
		snprintf(qualstr, 6, "_Q%d", jpegqual);
		qualstr[5]=0;
	}

	if((handle=tjInitDecompress())==NULL)
		_throwtj("executing tjInitDecompress()");

	bufsize=(yuv==YUVDECODE? yuvsize:pitch*scaledh);
	if(dstbuf==NULL)
	{
		if((dstbuf=(unsigned char *)malloc(bufsize)) == NULL)
			_throwunix("allocating image buffer");
		dstbufalloc=1;
	}
	/* Set the destination buffer to gray so we know whether the decompressor
	   attempted to write to it */
	memset(dstbuf, 127, bufsize);

	/* Execute once to preload cache */
	if(yuv==YUVDECODE)
	{
		if(tjDecompressToYUV(handle, jpegbuf[0], jpegsize[0], dstbuf, flags)==-1)
			_throwtj("executing tjDecompressToYUV()");
	}
	else if(tjDecompress2(handle, jpegbuf[0], jpegsize[0], dstbuf, scaledw,
		pitch, scaledh, pf, flags)==-1)
		_throwtj("executing tjDecompress2()");

	/* Benchmark */
	for(i=0, start=gettime(); (elapsed=gettime()-start)<benchtime; i++)
	{
		int tile=0;
		if(yuv==YUVDECODE)
		{
			if(tjDecompressToYUV(handle, jpegbuf[0], jpegsize[0], dstbuf, flags)==-1)
			_throwtj("executing tjDecompressToYUV()");
		}
		else for(row=0, dstptr=dstbuf; row<ntilesh; row++, dstptr+=pitch*tileh)
		{
			for(col=0, dstptr2=dstptr; col<ntilesw; col++, tile++, dstptr2+=ps*tilew)
			{
				int width=dotile? min(tilew, w-col*tilew):scaledw;
				int height=dotile? min(tileh, h-row*tileh):scaledh;
				if(tjDecompress2(handle, jpegbuf[tile], jpegsize[tile], dstptr2, width,
					pitch, height, pf, flags)==-1)
					_throwtj("executing tjDecompress2()");
			}
		}
	}

	if(tjDestroy(handle)==-1) _throwtj("executing tjDestroy()");
	handle=NULL;

	if(quiet)
	{
		printf("%s\n",
			sigfig((double)(w*h)/1000000.*(double)i/elapsed, 4, tempstr, 1024));
	}
	else
	{
		printf("D--> Frame rate:           %f fps\n", (double)i/elapsed);
		printf("     Dest. throughput:     %f Megapixels/sec\n",
			(double)(w*h)/1000000.*(double)i/elapsed);
	}
	if(yuv==YUVDECODE)
	{
		snprintf(tempstr, 1024, "%s_%s%s.yuv", filename, subName[subsamp],
			qualstr);
		if((file=fopen(tempstr, "wb"))==NULL)
			_throwunix("opening YUV image for output");
		if(fwrite(dstbuf, yuvsize, 1, file)!=1)
			_throwunix("writing YUV image");
		fclose(file);  file=NULL;
	}
	else
	{
		if(sf.num!=1 || sf.denom!=1)
			snprintf(sizestr, 20, "%d_%d", sf.num, sf.denom);
		else if(tilew!=w || tileh!=h)
			snprintf(sizestr, 20, "%dx%d", tilew, tileh);
		else snprintf(sizestr, 20, "full");
		if(decomponly)
			snprintf(tempstr, 1024, "%s_%s.%s", filename, sizestr, ext);
		else
			snprintf(tempstr, 1024, "%s_%s%s_%s.%s", filename, subName[subsamp],
				qualstr, sizestr, ext);
		if(savebmp(tempstr, dstbuf, scaledw, scaledh, pf,
			(flags&TJFLAG_BOTTOMUP)!=0)==-1)
			_throwbmp("saving bitmap");
		ptr=strrchr(tempstr, '.');
		snprintf(ptr, 1024-(ptr-tempstr), "-err.%s", ext);
		if(srcbuf && sf.num==1 && sf.denom==1)
		{
			if(!quiet) printf("Compression error written to %s.\n", tempstr);
			if(subsamp==TJ_GRAYSCALE)
			{
				int index, index2;
				for(row=0, index=0; row<h; row++, index+=pitch)
				{
					for(col=0, index2=index; col<w; col++, index2+=ps)
					{
						int rindex=index2+tjRedOffset[pf];
						int gindex=index2+tjGreenOffset[pf];
						int bindex=index2+tjBlueOffset[pf];
						int y=(int)((double)srcbuf[rindex]*0.299
							+ (double)srcbuf[gindex]*0.587
							+ (double)srcbuf[bindex]*0.114 + 0.5);
						if(y>255) y=255;  if(y<0) y=0;
						dstbuf[rindex]=abs(dstbuf[rindex]-y);
						dstbuf[gindex]=abs(dstbuf[gindex]-y);
						dstbuf[bindex]=abs(dstbuf[bindex]-y);
					}
				}
			}		
			else
			{
				for(row=0; row<h; row++)
					for(col=0; col<w*ps; col++)
						dstbuf[pitch*row+col]
							=abs(dstbuf[pitch*row+col]-srcbuf[pitch*row+col]);
			}
			if(savebmp(tempstr, dstbuf, w, h, pf,
				(flags&TJFLAG_BOTTOMUP)!=0)==-1)
				_throwbmp("saving bitmap");
		}
	}

	bailout:
	if(file) {fclose(file);  file=NULL;}
	if(handle) {tjDestroy(handle);  handle=NULL;}
	if(dstbuf && dstbufalloc) {free(dstbuf);  dstbuf=NULL;}
	return retval;
}


void dotestyuv(unsigned char *srcbuf, int w, int h, int subsamp,
	char *filename)
{
	char tempstr[1024], tempstr2[80];
	FILE *file=NULL;  tjhandle handle=NULL;
	unsigned char *dstbuf=NULL;
	double start, elapsed;
	int i, retval=0, ps=tjPixelSize[pf];
	int yuvsize=0;

	yuvsize=tjBufSizeYUV(w, h, subsamp);
	if((dstbuf=(unsigned char *)malloc(yuvsize)) == NULL)
		_throwunix("allocating image buffer");

	if(!quiet)
		printf(">>>>>  %s (%s) <--> YUV %s  <<<<<\n", pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "Bottom-up":"Top-down", subNameLong[subsamp]);

	if(quiet==1)
		printf("%s\t%s\t%s\tN/A\t", pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "BU":"TD", subNameLong[subsamp]);

	if((handle=tjInitCompress())==NULL)
		_throwtj("executing tjInitCompress()");

	/* Execute once to preload cache */
	if(tjEncodeYUV2(handle, srcbuf, w, 0, h, pf, dstbuf, subsamp, flags)==-1)
		_throwtj("executing tjEncodeYUV2()");

	/* Benchmark */
	for(i=0, start=gettime(); (elapsed=gettime()-start)<benchtime; i++)
	{
		if(tjEncodeYUV2(handle, srcbuf, w, 0, h, pf, dstbuf, subsamp, flags)==-1)
			_throwtj("executing tjEncodeYUV2()");
	}

	if(tjDestroy(handle)==-1) _throwtj("executing tjDestroy()");
	handle=NULL;

	if(quiet==1) printf("%-4d  %-4d\t", w, h);
	if(quiet)
	{
		printf("%s%c%s%c",
			sigfig((double)(w*h)/1000000.*(double)i/elapsed, 4, tempstr, 1024),
			quiet==2? '\n':'\t',
			sigfig((double)(w*h*ps)/(double)yuvsize, 4, tempstr2, 80),
			quiet==2? '\n':'\t');
	}
	else
	{
		printf("\n%s size: %d x %d\n", "Image", w, h);
		printf("C--> Frame rate:           %f fps\n", (double)i/elapsed);
		printf("     Output image size:    %d bytes\n", yuvsize);
		printf("     Compression ratio:    %f:1\n",
			(double)(w*h*ps)/(double)yuvsize);
		printf("     Source throughput:    %f Megapixels/sec\n",
			(double)(w*h)/1000000.*(double)i/elapsed);
		printf("     Output bit stream:    %f Megabits/sec\n",
			(double)yuvsize*8./1000000.*(double)i/elapsed);
	}
	snprintf(tempstr, 1024, "%s_%s.yuv", filename, subName[subsamp]);
	if((file=fopen(tempstr, "wb"))==NULL)
		_throwunix("opening reference image");
	if(fwrite(dstbuf, yuvsize, 1, file)!=1)
		_throwunix("writing reference image");
	fclose(file);  file=NULL;
	if(!quiet) printf("Reference image written to %s\n", tempstr);

	bailout:
	if(file) {fclose(file);  file=NULL;}
	if(dstbuf) {free(dstbuf);  dstbuf=NULL;}
	if(handle) {tjDestroy(handle);  handle=NULL;}
	return;
}


void dotest(unsigned char *srcbuf, int w, int h, int subsamp, int jpegqual,
	char *filename)
{
	char tempstr[1024], tempstr2[80];
	FILE *file=NULL;  tjhandle handle=NULL;
	unsigned char **jpegbuf=NULL, *tmpbuf=NULL, *srcptr, *srcptr2;
	double start, elapsed;
	int totaljpegsize=0, row, col, i, tilew=w, tileh=h, retval=0;
	unsigned long *jpegsize=NULL;
	int ps=tjPixelSize[pf], ntilesw=1, ntilesh=1, pitch=w*ps;

	if(yuv==YUVENCODE) {dotestyuv(srcbuf, w, h, subsamp, filename);  return;}

	if((tmpbuf=(unsigned char *)malloc(pitch*h)) == NULL)
		_throwunix("allocating temporary image buffer");

	if(!quiet)
		printf(">>>>>  %s (%s) <--> JPEG %s Q%d  <<<<<\n", pixFormatStr[pf],
			(flags&TJFLAG_BOTTOMUP)? "Bottom-up":"Top-down", subNameLong[subsamp],
			jpegqual);

	for(tilew=dotile? 8:w, tileh=dotile? 8:h; ; tilew*=2, tileh*=2)
	{
		if(tilew>w) tilew=w;  if(tileh>h) tileh=h;
		ntilesw=(w+tilew-1)/tilew;  ntilesh=(h+tileh-1)/tileh;

		if((jpegbuf=(unsigned char **)malloc(sizeof(unsigned char *)
			*ntilesw*ntilesh))==NULL)
			_throwunix("allocating JPEG tile array");
		memset(jpegbuf, 0, sizeof(unsigned char *)*ntilesw*ntilesh);
		if((jpegsize=(unsigned long *)malloc(sizeof(unsigned long)
			*ntilesw*ntilesh))==NULL)
			_throwunix("allocating JPEG size array");
		memset(jpegsize, 0, sizeof(unsigned long)*ntilesw*ntilesh);

		if((flags&TJFLAG_NOREALLOC)!=0)
			for(i=0; i<ntilesw*ntilesh; i++)
			{
				if((jpegbuf[i]=(unsigned char *)malloc(tjBufSize(tilew, tileh,
					subsamp)))==NULL)
					_throwunix("allocating JPEG tiles");
			}

		/* Compression test */
		if(quiet==1)
			printf("%s\t%s\t%s\t%d\t", pixFormatStr[pf],
				(flags&TJFLAG_BOTTOMUP)? "BU":"TD", subNameLong[subsamp], jpegqual);
		for(i=0; i<h; i++)
			memcpy(&tmpbuf[pitch*i], &srcbuf[w*ps*i], w*ps);
		if((handle=tjInitCompress())==NULL)
			_throwtj("executing tjInitCompress()");

		/* Execute once to preload cache */
		if(tjCompress2(handle, srcbuf, tilew, pitch, tileh, pf, &jpegbuf[0],
			&jpegsize[0], subsamp, jpegqual, flags)==-1)
			_throwtj("executing tjCompress2()");

		/* Benchmark */
		for(i=0, start=gettime(); (elapsed=gettime()-start)<benchtime; i++)
		{
			int tile=0;
			totaljpegsize=0;
			for(row=0, srcptr=srcbuf; row<ntilesh; row++, srcptr+=pitch*tileh)
			{
				for(col=0, srcptr2=srcptr; col<ntilesw; col++, tile++,
					srcptr2+=ps*tilew)
				{
					int width=min(tilew, w-col*tilew);
					int height=min(tileh, h-row*tileh);
					if(tjCompress2(handle, srcptr2, width, pitch, height, pf,
						&jpegbuf[tile], &jpegsize[tile], subsamp, jpegqual, flags)==-1)
						_throwtj("executing tjCompress()2");
					totaljpegsize+=jpegsize[tile];
				}
			}
		}

		if(tjDestroy(handle)==-1) _throwtj("executing tjDestroy()");
		handle=NULL;

		if(quiet==1) printf("%-4d  %-4d\t", tilew, tileh);
		if(quiet)
		{
			printf("%s%c%s%c",
				sigfig((double)(w*h)/1000000.*(double)i/elapsed, 4, tempstr, 1024),
				quiet==2? '\n':'\t',
				sigfig((double)(w*h*ps)/(double)totaljpegsize, 4, tempstr2, 80),
				quiet==2? '\n':'\t');
		}
		else
		{
			printf("\n%s size: %d x %d\n", dotile? "Tile":"Image", tilew,
				tileh);
			printf("C--> Frame rate:           %f fps\n", (double)i/elapsed);
			printf("     Output image size:    %d bytes\n", totaljpegsize);
			printf("     Compression ratio:    %f:1\n",
				(double)(w*h*ps)/(double)totaljpegsize);
			printf("     Source throughput:    %f Megapixels/sec\n",
				(double)(w*h)/1000000.*(double)i/elapsed);
			printf("     Output bit stream:    %f Megabits/sec\n",
				(double)totaljpegsize*8./1000000.*(double)i/elapsed);
		}
		if(tilew==w && tileh==h)
		{
			snprintf(tempstr, 1024, "%s_%s_Q%d.jpg", filename, subName[subsamp],
				jpegqual);
			if((file=fopen(tempstr, "wb"))==NULL)
				_throwunix("opening reference image");
			if(fwrite(jpegbuf[0], jpegsize[0], 1, file)!=1)
				_throwunix("writing reference image");
			fclose(file);  file=NULL;
			if(!quiet) printf("Reference image written to %s\n", tempstr);
		}

		/* Decompression test */
		if(decomptest(srcbuf, jpegbuf, jpegsize, tmpbuf, w, h, subsamp, jpegqual,
			filename, tilew, tileh)==-1)
			goto bailout;

		for(i=0; i<ntilesw*ntilesh; i++)
		{
			if(jpegbuf[i]) free(jpegbuf[i]);  jpegbuf[i]=NULL;
		}
		free(jpegbuf);  jpegbuf=NULL;
		free(jpegsize);  jpegsize=NULL;

		if(tilew==w && tileh==h) break;
	}

	bailout:
	if(file) {fclose(file);  file=NULL;}
	if(jpegbuf)
	{
		for(i=0; i<ntilesw*ntilesh; i++)
		{
			if(jpegbuf[i]) free(jpegbuf[i]);  jpegbuf[i]=NULL;
		}
		free(jpegbuf);  jpegbuf=NULL;
	}
	if(jpegsize) {free(jpegsize);  jpegsize=NULL;}
	if(tmpbuf) {free(tmpbuf);  tmpbuf=NULL;}
	if(handle) {tjDestroy(handle);  handle=NULL;}
	return;
}


void dodecomptest(char *filename)
{
	FILE *file=NULL;  tjhandle handle=NULL;
	unsigned char **jpegbuf=NULL, *srcbuf=NULL;
	unsigned long *jpegsize=NULL, srcsize, totaljpegsize;
	tjtransform *t=NULL;
	int w=0, h=0, subsamp=-1, _w, _h, _tilew, _tileh,
		_ntilesw, _ntilesh, _subsamp;
	char *temp=NULL, tempstr[80], tempstr2[80];
	int row, col, i, tilew, tileh, ntilesw=1, ntilesh=1, retval=0;
	double start, elapsed;
	int ps=tjPixelSize[pf], tile;

	if((file=fopen(filename, "rb"))==NULL)
		_throwunix("opening file");
	if(fseek(file, 0, SEEK_END)<0 || (srcsize=ftell(file))<0)
		_throwunix("determining file size");
	if((srcbuf=(unsigned char *)malloc(srcsize))==NULL)
		_throwunix("allocating memory");
	if(fseek(file, 0, SEEK_SET)<0)
		_throwunix("setting file position");
	if(fread(srcbuf, srcsize, 1, file)<1)
		_throwunix("reading JPEG data");
	fclose(file);  file=NULL;

	temp=strrchr(filename, '.');
	if(temp!=NULL) *temp='\0';

	if((handle=tjInitTransform())==NULL)
		_throwtj("executing tjInitTransform()");
	if(tjDecompressHeader2(handle, srcbuf, srcsize, &w, &h, &subsamp)==-1)
		_throwtj("executing tjDecompressHeader2()");

	if(quiet==1)
	{
		printf("All performance values in Mpixels/sec\n\n");
		printf("Bitmap\tBitmap\tJPEG\t%s %s \tXform\tComp\tDecomp\n",
			dotile? "Tile ":"Image", dotile? "Tile ":"Image");
		printf("Format\tOrder\tSubsamp\tWidth Height\tPerf \tRatio\tPerf\n\n");
	}
	else if(!quiet)
	{
		printf(">>>>>  JPEG %s --> %s (%s)  <<<<<\n", subNameLong[subsamp],
			pixFormatStr[pf], (flags&TJFLAG_BOTTOMUP)? "Bottom-up":"Top-down");
	}

	for(tilew=dotile? 16:w, tileh=dotile? 16:h; ; tilew*=2, tileh*=2)
	{
		if(tilew>w) tilew=w;  if(tileh>h) tileh=h;
		ntilesw=(w+tilew-1)/tilew;  ntilesh=(h+tileh-1)/tileh;

		if((jpegbuf=(unsigned char **)malloc(sizeof(unsigned char *)
			*ntilesw*ntilesh))==NULL)
			_throwunix("allocating JPEG tile array");
		memset(jpegbuf, 0, sizeof(unsigned char *)*ntilesw*ntilesh);
		if((jpegsize=(unsigned long *)malloc(sizeof(unsigned long)
			*ntilesw*ntilesh))==NULL)
			_throwunix("allocating JPEG size array");
		memset(jpegsize, 0, sizeof(unsigned long)*ntilesw*ntilesh);

		if((flags&TJFLAG_NOREALLOC)!=0)
			for(i=0; i<ntilesw*ntilesh; i++)
			{
				if((jpegbuf[i]=(unsigned char *)malloc(tjBufSize(tilew, tileh,
					subsamp)))==NULL)
					_throwunix("allocating JPEG tiles");
			}

		_w=w;  _h=h;  _tilew=tilew;  _tileh=tileh;
		if(!quiet)
		{
			printf("\n%s size: %d x %d", dotile? "Tile":"Image", _tilew,
				_tileh);
			if(sf.num!=1 || sf.denom!=1)
				printf(" --> %d x %d", TJSCALED(_w, sf), TJSCALED(_h, sf));
			printf("\n");
		}
		else if(quiet==1)
		{
			printf("%s\t%s\t%s\t", pixFormatStr[pf],
				(flags&TJFLAG_BOTTOMUP)? "BU":"TD", subNameLong[subsamp]);
			printf("%-4d  %-4d\t", tilew, tileh);
		}

		_subsamp=subsamp;
		if(dotile || xformop!=TJXOP_NONE || xformopt!=0 || customFilter)
		{
			if((t=(tjtransform *)malloc(sizeof(tjtransform)*ntilesw*ntilesh))
				==NULL)
				_throwunix("allocating image transform array");

			if(xformop==TJXOP_TRANSPOSE || xformop==TJXOP_TRANSVERSE
				|| xformop==TJXOP_ROT90 || xformop==TJXOP_ROT270)
			{
				_w=h;  _h=w;  _tilew=tileh;  _tileh=tilew;
			}

			if(xformopt&TJXOPT_GRAY) _subsamp=TJ_GRAYSCALE;
			if(xformop==TJXOP_HFLIP || xformop==TJXOP_ROT180)
				_w=_w-(_w%tjMCUWidth[_subsamp]);
			if(xformop==TJXOP_VFLIP || xformop==TJXOP_ROT180)
				_h=_h-(_h%tjMCUHeight[_subsamp]);
			if(xformop==TJXOP_TRANSVERSE || xformop==TJXOP_ROT90)
				_w=_w-(_w%tjMCUHeight[_subsamp]);
			if(xformop==TJXOP_TRANSVERSE || xformop==TJXOP_ROT270)
				_h=_h-(_h%tjMCUWidth[_subsamp]);
			_ntilesw=(_w+_tilew-1)/_tilew;
			_ntilesh=(_h+_tileh-1)/_tileh;

			for(row=0, tile=0; row<_ntilesh; row++)
			{
				for(col=0; col<_ntilesw; col++, tile++)
				{
					t[tile].r.w=min(_tilew, _w-col*_tilew);
					t[tile].r.h=min(_tileh, _h-row*_tileh);
					t[tile].r.x=col*_tilew;
					t[tile].r.y=row*_tileh;
					t[tile].op=xformop;
					t[tile].options=xformopt|TJXOPT_TRIM;
					t[tile].customFilter=customFilter;
					if(t[tile].options&TJXOPT_NOOUTPUT && jpegbuf[tile])
					{
						free(jpegbuf[tile]);  jpegbuf[tile]=NULL;
					}
				}
			}

			start=gettime();
			if(tjTransform(handle, srcbuf, srcsize, _ntilesw*_ntilesh, jpegbuf,
				jpegsize, t, flags)==-1)
				_throwtj("executing tjTransform()");
			elapsed=gettime()-start;

			free(t);  t=NULL;

			for(tile=0, totaljpegsize=0; tile<_ntilesw*_ntilesh; tile++)
				totaljpegsize+=jpegsize[tile];

			if(quiet)
			{
				printf("%s%c%s%c",
					sigfig((double)(w*h)/1000000./elapsed, 4, tempstr, 80),
					quiet==2? '\n':'\t',
					sigfig((double)(w*h*ps)/(double)totaljpegsize, 4, tempstr2, 80),
					quiet==2? '\n':'\t');
			}
			else if(!quiet)
			{
				printf("X--> Frame rate:           %f fps\n", 1.0/elapsed);
				printf("     Output image size:    %lu bytes\n", totaljpegsize);
				printf("     Compression ratio:    %f:1\n",
					(double)(w*h*ps)/(double)totaljpegsize);
				printf("     Source throughput:    %f Megapixels/sec\n",
					(double)(w*h)/1000000./elapsed);
				printf("     Output bit stream:    %f Megabits/sec\n",
					(double)totaljpegsize*8./1000000./elapsed);
			}
		}
		else
		{
			if(quiet==1) printf("N/A\tN/A\t");
			jpegsize[0]=srcsize;
			memcpy(jpegbuf[0], srcbuf, srcsize);
		}

		if(w==tilew) _tilew=_w;
		if(h==tileh) _tileh=_h;
		if(!(xformopt&TJXOPT_NOOUTPUT))
		{
			if(decomptest(NULL, jpegbuf, jpegsize, NULL, _w, _h, _subsamp, 0,
				filename, _tilew, _tileh)==-1)
				goto bailout;
		}
		else if(quiet==1) printf("N/A\n");

		for(i=0; i<ntilesw*ntilesh; i++)
		{
			free(jpegbuf[i]);  jpegbuf[i]=NULL;
		}
		free(jpegbuf);  jpegbuf=NULL;
		if(jpegsize) {free(jpegsize);  jpegsize=NULL;}

		if(tilew==w && tileh==h) break;
	}

	bailout:
	if(file) {fclose(file);  file=NULL;}
	if(jpegbuf)
	{
		for(i=0; i<ntilesw*ntilesh; i++)
		{
			if(jpegbuf[i]) free(jpegbuf[i]);  jpegbuf[i]=NULL;
		}
		free(jpegbuf);  jpegbuf=NULL;
	}
	if(jpegsize) {free(jpegsize);  jpegsize=NULL;}
	if(srcbuf) {free(srcbuf);  srcbuf=NULL;}
	if(t) {free(t);  t=NULL;}
	if(handle) {tjDestroy(handle);  handle=NULL;}
	return;
}


void usage(char *progname)
{
	int i;
	printf("USAGE: %s\n", progname);
	printf("       <Inputfile (BMP|PPM)> <Quality> [options]\n\n");
	printf("       %s\n", progname);
	printf("       <Inputfile (JPG)> [options]\n\n");
	printf("Options:\n\n");
	printf("-alloc = Dynamically allocate JPEG image buffers\n");
	printf("-bmp = Generate output images in Windows Bitmap format (default=PPM)\n");
	printf("-bottomup = Test bottom-up compression/decompression\n");
	printf("-tile = Test performance of the codec when the image is encoded as separate\n");
	printf("     tiles of varying sizes.\n");
	printf("-forcemmx, -forcesse, -forcesse2, -forcesse3 =\n");
	printf("     Force MMX, SSE, SSE2, or SSE3 code paths in the underlying codec\n");
	printf("-rgb, -bgr, -rgbx, -bgrx, -xbgr, -xrgb =\n");
	printf("     Test the specified color conversion path in the codec (default: BGR)\n");
	printf("-fastupsample = Use the fastest chrominance upsampling algorithm available in\n");
	printf("     the underlying codec\n");
	printf("-fastdct = Use the fastest DCT/IDCT algorithms available in the underlying\n");
	printf("     codec\n");
	printf("-accuratedct = Use the most accurate DCT/IDCT algorithms available in the\n");
	printf("     underlying codec\n");
	printf("-quiet = Output results in tabular rather than verbose format\n");
	printf("-yuvencode = Encode RGB input as planar YUV rather than compressing as JPEG\n");
	printf("-yuvdecode = Decode JPEG image to planar YUV rather than RGB\n");
	printf("-scale M/N = scale down the width/height of the decompressed JPEG image by a\n");
	printf("     factor of M/N (M/N = ");
	for(i=0; i<nsf; i++)
	{
		printf("%d/%d", scalingfactors[i].num, scalingfactors[i].denom);
		if(nsf==2 && i!=nsf-1) printf(" or ");
		else if(nsf>2)
		{
			if(i!=nsf-1) printf(", ");
			if(i==nsf-2) printf("or ");
		}
		if(i%8==0 && i!=0) printf("\n     ");
	}
	printf(")\n");
	printf("-hflip, -vflip, -transpose, -transverse, -rot90, -rot180, -rot270 =\n");
	printf("     Perform the corresponding lossless transform prior to\n");
	printf("     decompression (these options are mutually exclusive)\n");
	printf("-grayscale = Perform lossless grayscale conversion prior to decompression\n");
	printf("     test (can be combined with the other transforms above)\n");
	printf("-benchtime <t> = Run each benchmark for at least <t> seconds (default = 5.0)\n\n");
	printf("NOTE:  If the quality is specified as a range (e.g. 90-100), a separate\n");
	printf("test will be performed for all quality values in the range.\n\n");
	exit(1);
}


int main(int argc, char *argv[])
{
	unsigned char *srcbuf=NULL;  int w, h, i, j;
	int minqual=-1, maxqual=-1;  char *temp;
	int minarg=2;  int retval=0;

	if((scalingfactors=tjGetScalingFactors(&nsf))==NULL || nsf==0)
		_throwtj("executing tjGetScalingFactors()");

	if(argc<minarg) usage(argv[0]);

	temp=strrchr(argv[1], '.');
	if(temp!=NULL)
	{
		if(!strcasecmp(temp, ".bmp")) ext="bmp";
		if(!strcasecmp(temp, ".jpg") || !strcasecmp(temp, ".jpeg")) decomponly=1;
	}

	printf("\n");

	if(argc>minarg)
	{
		for(i=minarg; i<argc; i++)
		{
			if(!strcasecmp(argv[i], "-yuvencode"))
			{
				printf("Testing YUV planar encoding\n\n");
				yuv=YUVENCODE;  maxqual=minqual=100;
			}
			if(!strcasecmp(argv[i], "-yuvdecode"))
			{
				printf("Testing YUV planar decoding\n\n");
				yuv=YUVDECODE;
			}
		}
	}

	if(!decomponly && yuv!=YUVENCODE)
	{
		minarg=3;
		if(argc<minarg) usage(argv[0]);
		if((minqual=atoi(argv[2]))<1 || minqual>100)
		{
			puts("ERROR: Quality must be between 1 and 100.");
			exit(1);
		}
		if((temp=strchr(argv[2], '-'))!=NULL && strlen(temp)>1
			&& sscanf(&temp[1], "%d", &maxqual)==1 && maxqual>minqual && maxqual>=1
			&& maxqual<=100) {}
		else maxqual=minqual;
	}

	if(argc>minarg)
	{
		for(i=minarg; i<argc; i++)
		{
			if(!strcasecmp(argv[i], "-tile"))
			{
				dotile=1;  xformopt|=TJXOPT_CROP;
			}
			if(!strcasecmp(argv[i], "-forcesse3"))
			{
				printf("Forcing SSE3 code\n\n");
				flags|=TJFLAG_FORCESSE3;
			}
			if(!strcasecmp(argv[i], "-forcesse2"))
			{
				printf("Forcing SSE2 code\n\n");
				flags|=TJFLAG_FORCESSE2;
			}
			if(!strcasecmp(argv[i], "-forcesse"))
			{
				printf("Forcing SSE code\n\n");
				flags|=TJFLAG_FORCESSE;
			}
			if(!strcasecmp(argv[i], "-forcemmx"))
			{
				printf("Forcing MMX code\n\n");
				flags|=TJFLAG_FORCEMMX;
			}
			if(!strcasecmp(argv[i], "-fastupsample"))
			{
				printf("Using fast upsampling code\n\n");
				flags|=TJFLAG_FASTUPSAMPLE;
			}
			if(!strcasecmp(argv[i], "-fastdct"))
			{
				printf("Using fastest DCT/IDCT algorithm\n\n");
				flags|=TJFLAG_FASTDCT;
			}
			if(!strcasecmp(argv[i], "-accuratedct"))
			{
				printf("Using most accurate DCT/IDCT algorithm\n\n");
				flags|=TJFLAG_ACCURATEDCT;
			}
			if(!strcasecmp(argv[i], "-rgb")) pf=TJPF_RGB;
			if(!strcasecmp(argv[i], "-rgbx")) pf=TJPF_RGBX;
			if(!strcasecmp(argv[i], "-bgr")) pf=TJPF_BGR;
			if(!strcasecmp(argv[i], "-bgrx")) pf=TJPF_BGRX;
			if(!strcasecmp(argv[i], "-xbgr")) pf=TJPF_XBGR;
			if(!strcasecmp(argv[i], "-xrgb")) pf=TJPF_XRGB;
			if(!strcasecmp(argv[i], "-bottomup")) flags|=TJFLAG_BOTTOMUP;
			if(!strcasecmp(argv[i], "-quiet")) quiet=1;
			if(!strcasecmp(argv[i], "-qq")) quiet=2;
			if(!strcasecmp(argv[i], "-scale") && i<argc-1)
			{
				int temp1=0, temp2=0, match=0;
				if(sscanf(argv[++i], "%d/%d", &temp1, &temp2)==2)
				{
					for(j=0; j<nsf; j++)
					{
						if((double)temp1/(double)temp2
							== (double)scalingfactors[j].num/(double)scalingfactors[j].denom)
						{
							sf=scalingfactors[j];
							match=1;  break;
						}
					}
					if(!match) usage(argv[0]);
				}
				else usage(argv[0]);
			}
			if(!strcasecmp(argv[i], "-hflip")) xformop=TJXOP_HFLIP;
			if(!strcasecmp(argv[i], "-vflip")) xformop=TJXOP_VFLIP;
			if(!strcasecmp(argv[i], "-transpose")) xformop=TJXOP_TRANSPOSE;
			if(!strcasecmp(argv[i], "-transverse")) xformop=TJXOP_TRANSVERSE;
			if(!strcasecmp(argv[i], "-rot90")) xformop=TJXOP_ROT90;
			if(!strcasecmp(argv[i], "-rot180")) xformop=TJXOP_ROT180;
			if(!strcasecmp(argv[i], "-rot270")) xformop=TJXOP_ROT270;
			if(!strcasecmp(argv[i], "-grayscale")) xformopt|=TJXOPT_GRAY;
			if(!strcasecmp(argv[i], "-custom")) customFilter=dummyDCTFilter;
			if(!strcasecmp(argv[i], "-nooutput")) xformopt|=TJXOPT_NOOUTPUT;
			if(!strcasecmp(argv[i], "-benchtime") && i<argc-1)
			{
				double temp=atof(argv[++i]);
				if(temp>0.0) benchtime=temp;
				else usage(argv[0]);
			}
			if(!strcmp(argv[i], "-?")) usage(argv[0]);
			if(!strcasecmp(argv[i], "-alloc")) flags&=(~TJFLAG_NOREALLOC);
			if(!strcasecmp(argv[i], "-bmp")) ext="bmp";
		}
	}

	if((sf.num!=1 || sf.denom!=1) && dotile)
	{
		printf("Disabling tiled compression/decompression tests, because those tests do not\n");
		printf("work when scaled decompression is enabled.\n");
		dotile=0;
	}

	if(yuv && dotile)
	{
		printf("Disabling tiled compression/decompression tests, because those tests do not\n");
		printf("work when YUV encoding or decoding is enabled.\n\n");
		dotile=0;
	}

	if(!decomponly)
	{
		if(loadbmp(argv[1], &srcbuf, &w, &h, pf, (flags&TJFLAG_BOTTOMUP)!=0)==-1)
			_throwbmp("loading bitmap");
		temp=strrchr(argv[1], '.');
		if(temp!=NULL) *temp='\0';
	}

	if(quiet==1 && !decomponly)
	{
		printf("All performance values in Mpixels/sec\n\n");
		printf("Bitmap\tBitmap\tJPEG\tJPEG\t%s %s \tComp\tComp\tDecomp\n",
			dotile? "Tile ":"Image", dotile? "Tile ":"Image");
		printf("Format\tOrder\tSubsamp\tQual\tWidth Height\tPerf \tRatio\tPerf\n\n");
	}

	if(decomponly)
	{
		dodecomptest(argv[1]);
		printf("\n");
		goto bailout;
	}
	for(i=maxqual; i>=minqual; i--)
		dotest(srcbuf, w, h, TJ_GRAYSCALE, i, argv[1]);
	printf("\n");
	for(i=maxqual; i>=minqual; i--)
		dotest(srcbuf, w, h, TJ_420, i, argv[1]);
	printf("\n");
	for(i=maxqual; i>=minqual; i--)
		dotest(srcbuf, w, h, TJ_422, i, argv[1]);
	printf("\n");
	for(i=maxqual; i>=minqual; i--)
		dotest(srcbuf, w, h, TJ_444, i, argv[1]);
	printf("\n");

	bailout:
	if(srcbuf) free(srcbuf);
	return retval;
}
