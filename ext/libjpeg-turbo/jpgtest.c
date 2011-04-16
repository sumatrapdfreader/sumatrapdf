/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005, 2006 Sun Microsystems, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include "./bmp.h"
#include "./rrutil.h"
#include "./rrtimer.h"
#include "./turbojpeg.h"

#define _throw(op, err) {  \
	printf("ERROR in line %d while %s:\n%s\n", __LINE__, op, err);  goto bailout;}
#define _throwunix(m) _throw(m, strerror(errno))
#define _throwtj(m) _throw(m, tjGetErrorStr())
#define _throwbmp(m) _throw(m, bmpgeterr())

#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))

enum {YUVENCODE=1, YUVDECODE};
int forcemmx=0, forcesse=0, forcesse2=0, forcesse3=0, fastupsample=0,
	decomponly=0, yuv=0;
const int _ps[BMPPIXELFORMATS]={3, 4, 3, 4, 4, 4};
const int _flags[BMPPIXELFORMATS]={0, 0, TJ_BGR, TJ_BGR,
	TJ_BGR|TJ_ALPHAFIRST, TJ_ALPHAFIRST};
const int _rindex[BMPPIXELFORMATS]={0, 0, 2, 2, 3, 1};
const int _gindex[BMPPIXELFORMATS]={1, 1, 1, 1, 2, 2};
const int _bindex[BMPPIXELFORMATS]={2, 2, 0, 0, 1, 3};
const char *_pfname[]={"RGB", "RGBX", "BGR", "BGRX", "XBGR", "XRGB"};
const char *_subnamel[NUMSUBOPT]={"4:4:4", "4:2:2", "4:2:0", "GRAY"};
const char *_subnames[NUMSUBOPT]={"444", "422", "420", "GRAY"};

void printsigfig(double val, int figs)
{
	char format[80];
	double _l=log10(val);  int l;
	if(_l<0.)
	{
		l=(int)fabs(_l);
		sprintf(format, "%%%d.%df", figs+l+2, figs+l);
	}
	else
	{
		l=(int)_l+1;
		if(figs<=l) sprintf(format, "%%.0f");
		else sprintf(format, "%%%d.%df", figs+1, figs-l);
	}	
	printf(format, val);
}

void dotest(unsigned char *srcbuf, int w, int h, int pf, int bu,
	int jpegsub, int qual, char *filename, int dotile, int useppm, int quiet)
{
	char tempstr[1024];
	FILE *outfile=NULL;  tjhandle hnd=NULL;
	unsigned char **jpegbuf=NULL, *rgbbuf=NULL;
	double start, elapsed;
	int jpgbufsize=0, i, j, tilesizex=w, tilesizey=h, numtilesx=1, numtilesy=1,
		ITER;
	unsigned long *comptilesize=NULL;
	int flags=(forcemmx?TJ_FORCEMMX:0)|(forcesse?TJ_FORCESSE:0)
		|(forcesse2?TJ_FORCESSE2:0)|(forcesse3?TJ_FORCESSE3:0)
		|(fastupsample?TJ_FASTUPSAMPLE:0);
	int ps=_ps[pf], tilen;
	int pitch=w*ps, yuvsize=0;

	flags |= _flags[pf];
	if(bu) flags |= TJ_BOTTOMUP;

	if(yuv==YUVENCODE) yuvsize=TJBUFSIZEYUV(w, h, jpegsub);
	if((rgbbuf=(unsigned char *)malloc(max(yuvsize, pitch*h+1))) == NULL)
		_throwunix("allocating image buffer");

	if(!quiet)
	{
		if(yuv==YUVENCODE)
			printf(">>>>>  %s (%s) <--> YUV %s  <<<<<\n", _pfname[pf],
				bu?"Bottom-up":"Top-down", _subnamel[jpegsub]);
		else
			printf(">>>>>  %s (%s) <--> JPEG %s Q%d  <<<<<\n", _pfname[pf],
				bu?"Bottom-up":"Top-down", _subnamel[jpegsub], qual);
	}
	if(yuv) dotile=0;
	if(dotile) {tilesizex=tilesizey=4;}  else {tilesizex=w;  tilesizey=h;}

	do
	{
		tilesizex*=2;  if(tilesizex>w) tilesizex=w;
		tilesizey*=2;  if(tilesizey>h) tilesizey=h;
		numtilesx=(w+tilesizex-1)/tilesizex;
		numtilesy=(h+tilesizey-1)/tilesizey;
		if((comptilesize=(unsigned long *)malloc(sizeof(unsigned long)*numtilesx*numtilesy)) == NULL
		|| (jpegbuf=(unsigned char **)malloc(sizeof(unsigned char *)*numtilesx*numtilesy)) == NULL)
			_throwunix("allocating image buffers");
		memset(jpegbuf, 0, sizeof(unsigned char *)*numtilesx*numtilesy);
		for(i=0; i<numtilesx*numtilesy; i++)
		{
			if((jpegbuf[i]=(unsigned char *)malloc(
				yuv==YUVENCODE? TJBUFSIZEYUV(tilesizex, tilesizey, jpegsub)
					: TJBUFSIZE(tilesizex, tilesizey))) == NULL)
				_throwunix("allocating image buffers");
		}

		// Compression test
		if(quiet==1) printf("%s\t%s\t%s\t%d\t",  _pfname[pf], bu?"BU":"TD",
			_subnamel[jpegsub], qual);
		for(i=0; i<h; i++) memcpy(&rgbbuf[pitch*i], &srcbuf[w*ps*i], w*ps);
		if((hnd=tjInitCompress())==NULL)
			_throwtj("executing tjInitCompress()");
		if(yuv==YUVENCODE)
		{
			if(tjEncodeYUV(hnd, rgbbuf, tilesizex, pitch, tilesizey, ps,
				jpegbuf[0], jpegsub, flags)==-1)
				_throwtj("executing tjEncodeYUV()");
			comptilesize[0]=TJBUFSIZEYUV(tilesizex, tilesizey, jpegsub);
		}
		else if(tjCompress(hnd, rgbbuf, tilesizex, pitch, tilesizey, ps,
			jpegbuf[0], &comptilesize[0], jpegsub, qual, flags)==-1)
			_throwtj("executing tjCompress()");
		ITER=0;
		start=rrtime();
		do
		{
			jpgbufsize=0;  tilen=0;
			for(i=0; i<h; i+=tilesizey)
			{
				for(j=0; j<w; j+=tilesizex)
				{
					int tempw=min(tilesizex, w-j), temph=min(tilesizey, h-i);
					if(yuv==YUVENCODE)
					{
						if(tjEncodeYUV(hnd, &rgbbuf[pitch*i+j*ps], tempw, pitch,
							temph, ps, jpegbuf[tilen], jpegsub, flags)==-1)
							_throwtj("executing tjEncodeYUV()");
						comptilesize[tilen]=TJBUFSIZEYUV(tempw, temph, jpegsub);
					}
					else if(tjCompress(hnd, &rgbbuf[pitch*i+j*ps], tempw, pitch,
						temph, ps, jpegbuf[tilen], &comptilesize[tilen], jpegsub, qual,
						flags)==-1)
						_throwtj("executing tjCompress()");
					jpgbufsize+=comptilesize[tilen];
					tilen++;
				}
			}
			ITER++;
		} while((elapsed=rrtime()-start)<5.);
		if(tjDestroy(hnd)==-1) _throwtj("executing tjDestroy()");
		hnd=NULL;
		if(quiet==1)
		{
			if(tilesizex==w && tilesizey==h) printf("Full     \t");
			else printf("%-4d %-4d\t", tilesizex, tilesizey);
		}
		if(quiet)
		{
			printsigfig((double)(w*h)/1000000.*(double)ITER/elapsed, 4);
			printf("%c", quiet==2? '\n':'\t');
			printsigfig((double)(w*h*ps)/(double)jpgbufsize, 4);
			printf("%c", quiet==2? '\n':'\t');
		}
		else
		{
			if(tilesizex==w && tilesizey==h) printf("\nFull image\n");
			else printf("\nTile size: %d x %d\n", tilesizex, tilesizey);
			printf("C--> Frame rate:           %f fps\n", (double)ITER/elapsed);
			printf("     Output image size:    %d bytes\n", jpgbufsize);
			printf("     Compression ratio:    %f:1\n",
				(double)(w*h*ps)/(double)jpgbufsize);
			printf("     Source throughput:    %f Megapixels/sec\n",
				(double)(w*h)/1000000.*(double)ITER/elapsed);
			printf("     Output bit stream:    %f Megabits/sec\n",
				(double)jpgbufsize*8./1000000.*(double)ITER/elapsed);
		}
		if(tilesizex==w && tilesizey==h)
		{
			if(yuv==YUVENCODE)
				sprintf(tempstr, "%s_%s.yuv", filename, _subnames[jpegsub]);
			else
				sprintf(tempstr, "%s_%sQ%d.jpg", filename, _subnames[jpegsub], qual);
			if((outfile=fopen(tempstr, "wb"))==NULL)
				_throwunix("opening reference image");
			if(fwrite(jpegbuf[0], jpgbufsize, 1, outfile)!=1)
				_throwunix("writing reference image");
			fclose(outfile);  outfile=NULL;
			if(!quiet) printf("Reference image written to %s\n", tempstr);
		}
		if(yuv==YUVENCODE)
		{
			if(quiet==1) printf("\n");  goto bailout;
		}

		// Decompression test
		memset(rgbbuf, 127, max(yuvsize, pitch*h));  // Grey image means decompressor did nothing
		if((hnd=tjInitDecompress())==NULL)
			_throwtj("executing tjInitDecompress()");
		if(yuv==YUVDECODE)
		{
			if(tjDecompressToYUV(hnd, jpegbuf[0], jpgbufsize, rgbbuf, flags)==-1)
				_throwtj("executing tjDecompressToYUV()");
		}
		else if(tjDecompress(hnd, jpegbuf[0], jpgbufsize, rgbbuf, tilesizex, pitch,
			tilesizey, ps, flags)==-1)
			_throwtj("executing tjDecompress()");
		ITER=0;
		start=rrtime();
		do
		{
			int tilen=0;
			for(i=0; i<h; i+=tilesizey)
			{
				for(j=0; j<w; j+=tilesizex)
				{
					int tempw=min(tilesizex, w-j), temph=min(tilesizey, h-i);
					if(yuv==YUVDECODE)
					{
						if(tjDecompressToYUV(hnd, jpegbuf[tilen], comptilesize[tilen],
							&rgbbuf[pitch*i+ps*j], flags)==-1)
							_throwtj("executing tjDecompressToYUV()");
					}
					else if(tjDecompress(hnd, jpegbuf[tilen], comptilesize[tilen],
						&rgbbuf[pitch*i+ps*j], tempw, pitch, temph, ps, flags)==-1)
						_throwtj("executing tjDecompress()");
					tilen++;
				}
			}
			ITER++;
		}	while((elapsed=rrtime()-start)<5.);
		if(tjDestroy(hnd)==-1) _throwtj("executing tjDestroy()");
		hnd=NULL;
		if(quiet)
		{
			printsigfig((double)(w*h)/1000000.*(double)ITER/elapsed, 4);
			printf("\n");
		}
		else
		{
			printf("D--> Frame rate:           %f fps\n", (double)ITER/elapsed);
			printf("     Dest. throughput:     %f Megapixels/sec\n",
				(double)(w*h)/1000000.*(double)ITER/elapsed);
		}
		if(yuv==YUVDECODE)
		{
			sprintf(tempstr, "%s_%sQ%d.yuv", filename, _subnames[jpegsub], qual);
			if((outfile=fopen(tempstr, "wb"))==NULL)
				_throwunix("opening YUV image for output");
			if(fwrite(rgbbuf, yuvsize, 1, outfile)!=1)
				_throwunix("writing YUV image");
			fclose(outfile);  outfile=NULL;
		}
		else
		{
			if(tilesizex==w && tilesizey==h)
				sprintf(tempstr, "%s_%sQ%d_full.%s", filename, _subnames[jpegsub], qual,
					useppm?"ppm":"bmp");
			else sprintf(tempstr, "%s_%sQ%d_%dx%d.%s", filename, _subnames[jpegsub],
				qual, tilesizex, tilesizey, useppm?"ppm":"bmp");
			if(savebmp(tempstr, rgbbuf, w, h, pf, pitch, bu)==-1)
				_throwbmp("saving bitmap");
			sprintf(strrchr(tempstr, '.'), "-err.%s", useppm?"ppm":"bmp");
			if(!quiet)
				printf("Computing compression error and saving to %s.\n", tempstr);
			if(jpegsub==TJ_GRAYSCALE)
			{
				for(j=0; j<h; j++)
				{
					for(i=0; i<w*ps; i+=ps)
					{
						int y=(int)((double)srcbuf[w*ps*j+i+_rindex[pf]]*0.299
							+ (double)srcbuf[w*ps*j+i+_gindex[pf]]*0.587
							+ (double)srcbuf[w*ps*j+i+_bindex[pf]]*0.114 + 0.5);
						if(y>255) y=255;  if(y<0) y=0;
						rgbbuf[pitch*j+i+_rindex[pf]]=abs(rgbbuf[pitch*j+i+_rindex[pf]]-y);
						rgbbuf[pitch*j+i+_gindex[pf]]=abs(rgbbuf[pitch*j+i+_gindex[pf]]-y);
						rgbbuf[pitch*j+i+_bindex[pf]]=abs(rgbbuf[pitch*j+i+_bindex[pf]]-y);
					}
				}
			}		
			else
			{
				for(j=0; j<h; j++) for(i=0; i<w*ps; i++)
					rgbbuf[pitch*j+i]=abs(rgbbuf[pitch*j+i]-srcbuf[w*ps*j+i]);
			}
			if(savebmp(tempstr, rgbbuf, w, h, pf, pitch, bu)==-1)
				_throwbmp("saving bitmap");
		}

		// Cleanup
		if(outfile) {fclose(outfile);  outfile=NULL;}
		if(jpegbuf)
		{
			for(i=0; i<numtilesx*numtilesy; i++)
				{if(jpegbuf[i]) free(jpegbuf[i]);  jpegbuf[i]=NULL;}
			free(jpegbuf);  jpegbuf=NULL;
		}
		if(comptilesize) {free(comptilesize);  comptilesize=NULL;}
	} while(tilesizex<w || tilesizey<h);

	if(rgbbuf) {free(rgbbuf);  rgbbuf=NULL;}
	return;

	bailout:
	if(outfile) {fclose(outfile);  outfile=NULL;}
	if(jpegbuf)
	{
		for(i=0; i<numtilesx*numtilesy; i++)
			{if(jpegbuf[i]) free(jpegbuf[i]);  jpegbuf[i]=NULL;}
		free(jpegbuf);  jpegbuf=NULL;
	}
	if(comptilesize) {free(comptilesize);  comptilesize=NULL;}
	if(rgbbuf) {free(rgbbuf);  rgbbuf=NULL;}
	if(hnd) {tjDestroy(hnd);  hnd=NULL;}
	return;
}


void dodecomptest(char *filename, int pf, int bu, int useppm,
	int quiet)
{
	char tempstr[1024];
	FILE *file=NULL;  tjhandle hnd=NULL;
	unsigned char *jpegbuf=NULL, *rgbbuf=NULL;
	double start, elapsed;
	int w, h, ITER;
	unsigned long jpgbufsize=0;
	int flags=(forcemmx?TJ_FORCEMMX:0)|(forcesse?TJ_FORCESSE:0)
		|(forcesse2?TJ_FORCESSE2:0)|(forcesse3?TJ_FORCESSE3:0)
		|(fastupsample?TJ_FASTUPSAMPLE:0);
	int ps=_ps[pf], pitch, jpegsub=-1;
	char *temp=NULL;
	int yuvsize, bufsize;

	flags |= _flags[pf];
	if(bu) flags |= TJ_BOTTOMUP;

	if((file=fopen(filename, "rb"))==NULL)
		_throwunix("opening file");
	if(fseek(file, 0, SEEK_END)<0 || (jpgbufsize=ftell(file))<0)
		_throwunix("determining file size");
	if((jpegbuf=(unsigned char *)malloc(jpgbufsize))==NULL)
		_throwunix("allocating memory");
	if(fseek(file, 0, SEEK_SET)<0)
		_throwunix("setting file position");
	if(fread(jpegbuf, jpgbufsize, 1, file)<1)
		_throwunix("reading JPEG data");
	fclose(file);  file=NULL;

	temp=strrchr(filename, '.');
	if(temp!=NULL) *temp='\0';

	if((hnd=tjInitDecompress())==NULL) _throwtj("executing tjInitDecompress()");
	if(tjDecompressHeader2(hnd, jpegbuf, jpgbufsize, &w, &h, &jpegsub)==-1)
		_throwtj("executing tjDecompressHeader2()");

	yuvsize=TJBUFSIZEYUV(w, h, jpegsub);

	pitch=w*ps;

	if(quiet==1)
	{
		printf("All performance values in Mpixels/sec\n\n");
		printf("Bitmap\tBitmap\tImage Size\tDecomp\n"),
		printf("Format\tOrder\t X    Y  \tPerf\n\n");
		printf("%s\t%s\t%-4d %-4d\t", _pfname[pf], bu?"BU":"TD", w, h);
	}

	bufsize=(yuv==YUVDECODE? yuvsize:pitch*h);
	if((rgbbuf=(unsigned char *)malloc(bufsize))==NULL)
		_throwunix("allocating image buffer");

	if(!quiet)
	{
		if(yuv==YUVDECODE)
			printf(">>>>>  JPEG --> YUV %s  <<<<<\n", _subnamel[jpegsub]);
		else
			printf(">>>>>  JPEG --> %s (%s)  <<<<<\n", _pfname[pf],
				bu?"Bottom-up":"Top-down");
		printf("\nImage size: %d x %d\n", w, h);
	}

	memset(rgbbuf, 127, bufsize);  // Grey image means decompressor did nothing
	if(yuv==YUVDECODE)
	{
		if(tjDecompressToYUV(hnd, jpegbuf, jpgbufsize, rgbbuf, flags)==-1)
			_throwtj("executing tjDecompressToYUV()");
	}
	else if(tjDecompress(hnd, jpegbuf, jpgbufsize, rgbbuf, w, pitch, h, ps,
		flags)==-1)
		_throwtj("executing tjDecompress()");
	ITER=0;
	start=rrtime();
	do
	{
		if(yuv==YUVDECODE)
		{
			if(tjDecompressToYUV(hnd, jpegbuf, jpgbufsize, rgbbuf, flags)==-1)
				_throwtj("executing tjDecompressToYUV()");
		}
		else if(tjDecompress(hnd, jpegbuf, jpgbufsize, rgbbuf, w, pitch, h, ps,
			flags)==-1)
			_throwtj("executing tjDecompress()");
		ITER++;
	}	while((elapsed=rrtime()-start)<5.);
	if(tjDestroy(hnd)==-1) _throwtj("executing tjDestroy()");
	hnd=NULL;
	if(quiet)
	{
		printsigfig((double)(w*h)/1000000.*(double)ITER/elapsed, 4);
		printf("\n");
	}
	else
	{
		printf("D--> Frame rate:           %f fps\n", (double)ITER/elapsed);
		printf("     Dest. throughput:     %f Megapixels/sec\n",
			(double)(w*h)/1000000.*(double)ITER/elapsed);
	}
	sprintf(tempstr, "%s_full.%s", filename, useppm?"ppm":"bmp");
	if(yuv==YUVDECODE)
	{
		sprintf(tempstr, "%s_%s.yuv", filename, _subnames[jpegsub]);
		if((file=fopen(tempstr, "wb"))==NULL)
			_throwunix("opening YUV image for output");
		if(fwrite(rgbbuf, yuvsize, 1, file)!=1)
			_throwunix("writing YUV image");
		fclose(file);  file=NULL;
	}
	else
	{
		if(savebmp(tempstr, rgbbuf, w, h, pf, pitch, bu)==-1)
			_throwbmp("saving bitmap");
	}

	bailout:
	if(file) {fclose(file);  file=NULL;}
	if(jpegbuf) {free(jpegbuf);  jpegbuf=NULL;}
	if(rgbbuf) {free(rgbbuf);  rgbbuf=NULL;}
	if(hnd) {tjDestroy(hnd);  hnd=NULL;}
	return;
}


void usage(char *progname)
{
	printf("USAGE: %s <Inputfile (BMP|PPM))> <%% Quality>\n", progname);
	printf("       %s <Inputfile (JPG))>\n\n", progname);
	printf("       [-tile]\n");
	printf("       Test performance of the codec when the image is encoded\n");
	printf("       as separate tiles of varying sizes.\n\n");
	printf("       [-forcemmx] [-forcesse] [-forcesse2] [-forcesse3]\n");
	printf("       Force MMX, SSE, SSE2, or SSE3 code paths in the underlying codec\n\n");
	printf("       [-rgb | -bgr | -rgbx | -bgrx | -xbgr | -xrgb]\n");
	printf("       Test the specified color conversion path in the codec (default: BGR)\n\n");
	printf("       [-fastupsample]\n");
	printf("       Use fast, inaccurate upsampling code to perform 4:2:2 and 4:2:0\n");
	printf("       YUV decoding in libjpeg decompressor\n\n");
	printf("       [-quiet]\n");
	printf("       Output in tabular rather than verbose format\n\n");
	printf("       [-yuvencode]\n");
	printf("       Encode RGB input as planar YUV rather than compressing as JPEG\n\n");
	printf("       [-yuvdecode]\n");
	printf("       Decode JPEG image to planar YUV rather than RGB\n\n");
	printf("       NOTE: If the quality is specified as a range, i.e. 90-100, a separate\n");
	printf("       test will be performed for all quality values in the range.\n");
	exit(1);
}


int main(int argc, char *argv[])
{
	unsigned char *bmpbuf=NULL;  int w, h, i, useppm=0;
	int qual=-1, dotile=0, quiet=0, hiqual=-1;  char *temp;
	int pf=BMP_BGR;
	int bu=0, minarg=2;

	if(argc<minarg) usage(argv[0]);

	temp=strrchr(argv[1], '.');
	if(temp!=NULL)
	{
		if(!stricmp(temp, ".ppm")) useppm=1;
		if(!stricmp(temp, ".jpg") || !stricmp(temp, ".jpeg")) decomponly=1;
	}

	printf("\n");

	if(argc>minarg)
	{
		for(i=minarg; i<argc; i++)
		{
			if(!stricmp(argv[i], "-yuvencode"))
			{
				printf("Testing YUV planar encoding\n\n");
				yuv=YUVENCODE;  hiqual=qual=100;
			}
			if(!stricmp(argv[i], "-yuvdecode"))
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
		if((qual=atoi(argv[2]))<1 || qual>100)
		{
			puts("ERROR: Quality must be between 1 and 100.");
			exit(1);
		}
		if((temp=strchr(argv[2], '-'))!=NULL && strlen(temp)>1
			&& sscanf(&temp[1], "%d", &hiqual)==1 && hiqual>qual && hiqual>=1
			&& hiqual<=100) {}
		else hiqual=qual;
	}

	if(argc>minarg)
	{
		for(i=minarg; i<argc; i++)
		{
			if(!stricmp(argv[i], "-tile")) dotile=1;
			if(!stricmp(argv[i], "-forcesse3"))
			{
				printf("Using SSE3 code\n\n");
				forcesse3=1;
			}
			if(!stricmp(argv[i], "-forcesse2"))
			{
				printf("Using SSE2 code\n\n");
				forcesse2=1;
			}
			if(!stricmp(argv[i], "-forcesse"))
			{
				printf("Using SSE code\n\n");
				forcesse=1;
			}
			if(!stricmp(argv[i], "-forcemmx"))
			{
				printf("Using MMX code\n\n");
				forcemmx=1;
			}
			if(!stricmp(argv[i], "-fastupsample"))
			{
				printf("Using fast upsampling code\n\n");
				fastupsample=1;
			}
			if(!stricmp(argv[i], "-rgb")) pf=BMP_RGB;
			if(!stricmp(argv[i], "-rgbx")) pf=BMP_RGBX;
			if(!stricmp(argv[i], "-bgr")) pf=BMP_BGR;
			if(!stricmp(argv[i], "-bgrx")) pf=BMP_BGRX;
			if(!stricmp(argv[i], "-xbgr")) pf=BMP_XBGR;
			if(!stricmp(argv[i], "-xrgb")) pf=BMP_XRGB;
			if(!stricmp(argv[i], "-bottomup")) bu=1;
			if(!stricmp(argv[i], "-quiet")) quiet=1;
			if(!stricmp(argv[i], "-qq")) quiet=2;
		}
	}

	if(!decomponly)
	{
		if(loadbmp(argv[1], &bmpbuf, &w, &h, pf, 1, bu)==-1)
			_throwbmp("loading bitmap");
		temp=strrchr(argv[1], '.');
		if(temp!=NULL) *temp='\0';
	}

	if(quiet==1 && !decomponly)
	{
		printf("All performance values in Mpixels/sec\n\n");
		printf("Bitmap\tBitmap\tJPEG\tJPEG\tTile Size\tCompr\tCompr\tDecomp\n");
		printf("Format\tOrder\tFormat\tQual\t X    Y  \tPerf \tRatio\tPerf\n\n");
	}

	if(decomponly)
	{
		dodecomptest(argv[1], pf, bu, 1, quiet);
		printf("\n");
		goto bailout;
	}
	for(i=hiqual; i>=qual; i--)
		dotest(bmpbuf, w, h, pf, bu, TJ_GRAYSCALE, i, argv[1], dotile, useppm, quiet);
	printf("\n");
	for(i=hiqual; i>=qual; i--)
		dotest(bmpbuf, w, h, pf, bu, TJ_420, i, argv[1], dotile, useppm, quiet);
	printf("\n");
	for(i=hiqual; i>=qual; i--)
		dotest(bmpbuf, w, h, pf, bu, TJ_422, i, argv[1], dotile, useppm, quiet);
	printf("\n");
	for(i=hiqual; i>=qual; i--)
		dotest(bmpbuf, w, h, pf, bu, TJ_444, i, argv[1], dotile, useppm, quiet);
	printf("\n");

	bailout:
	if(bmpbuf) free(bmpbuf);
	return 0;
}
