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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./rrtimer.h"
#include "./turbojpeg.h"
#ifndef _WIN32
 #define stricmp strcasecmp
#endif

#define _catch(f) {if((f)==-1) {printf("TJPEG: %s\n", tjGetErrorStr());  bailout();}}

const char *_subnamel[NUMSUBOPT]={"4:4:4", "4:2:2", "4:2:0", "GRAY"};
const char *_subnames[NUMSUBOPT]={"444", "422", "420", "GRAY"};
const int _hsf[NUMSUBOPT]={1, 2, 2, 1};
const int _vsf[NUMSUBOPT]={1, 1, 2, 1};

enum {YUVENCODE=1, YUVDECODE};
int yuv=0;

int exitstatus=0;
#define bailout() {exitstatus=-1;  goto finally;}

int pixels[9][3]=
{
	{0, 255, 0},
	{255, 0, 255},
	{255, 255, 0},
	{0, 0, 255},
	{0, 255, 255},
	{255, 0, 0},
	{255, 255, 255},
	{0, 0, 0},
	{255, 0, 0}
};

void initbuf(unsigned char *buf, int w, int h, int ps, int flags)
{
	int roffset=(flags&TJ_BGR)?2:0, goffset=1, boffset=(flags&TJ_BGR)?0:2, i,
		_i, j;
	if(flags&TJ_ALPHAFIRST) {roffset++;  goffset++;  boffset++;}
	memset(buf, 0, w*h*ps);
	if(ps==1)
	{
		for(_i=0; _i<16; _i++)
		{
			if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
			for(j=0; j<w; j++)
			{
				if(((_i/8)+(j/8))%2==0) buf[w*i+j]=255;
				else buf[w*i+j]=76;
			}
		}
		for(_i=16; _i<h; _i++)
		{
			if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
			for(j=0; j<w; j++)
			{
				if(((_i/8)+(j/8))%2==0) buf[w*i+j]=0;
				else buf[w*i+j]=226;
			}
		}
		return;
	}
	for(_i=0; _i<16; _i++)
	{
		if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
		for(j=0; j<w; j++)
		{
			buf[(w*i+j)*ps+roffset]=255;
			if(((_i/8)+(j/8))%2==0)
			{
				buf[(w*i+j)*ps+goffset]=255;
				buf[(w*i+j)*ps+boffset]=255;
			}
		}
	}
	for(_i=16; _i<h; _i++)
	{
		if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
		for(j=0; j<w; j++)
		{
			if(((_i/8)+(j/8))%2!=0)
			{
				buf[(w*i+j)*ps+roffset]=255;
				buf[(w*i+j)*ps+goffset]=255;
			}
		}
	}
}

void dumpbuf(unsigned char *buf, int w, int h, int ps, int flags)
{
	int roffset=(flags&TJ_BGR)?2:0, goffset=1, boffset=(flags&TJ_BGR)?0:2, i,
		j;
	printf("\n");
	for(i=0; i<h; i++)
	{
		for(j=0; j<w; j++)
		{
			printf("%.3d/%.3d/%.3d ", buf[(w*i+j)*ps+roffset],
				buf[(w*i+j)*ps+goffset], buf[(w*i+j)*ps+boffset]);
		}
		printf("\n");
	}
}

int checkbuf(unsigned char *buf, int w, int h, int ps, int subsamp, int flags)
{
	int roffset=(flags&TJ_BGR)?2:0, goffset=1, boffset=(flags&TJ_BGR)?0:2, i,
		_i, j;
	if(flags&TJ_ALPHAFIRST) {roffset++;  goffset++;  boffset++;}
	if(ps==1) roffset=goffset=boffset=0;
	if(subsamp==TJ_GRAYSCALE)
	{
		for(_i=0; _i<16; _i++)
		{
			if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
			for(j=0; j<w; j++)
			{
				unsigned char r=buf[(w*i+j)*ps+roffset],
					g=buf[(w*i+j)*ps+goffset],
					b=buf[(w*i+j)*ps+boffset];
				if(((_i/8)+(j/8))%2==0)
				{
					if(r<253 || g<253 || b<253) return 0;
				}
				else
				{
					if(r<74 || r>78 || g<74 || g>78 || b<74 || b>78) return 0;
				}
			}
		}
		for(_i=16; _i<h; _i++)
		{
			if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
			for(j=0; j<w; j++)
			{
				unsigned char r=buf[(w*i+j)*ps+roffset],
					g=buf[(w*i+j)*ps+goffset],
					b=buf[(w*i+j)*ps+boffset];
				if(((_i/8)+(j/8))%2==0)
				{
					if(r>2 || g>2 || b>2) return 0;
				}
				else
				{
					if(r<224 || r>228 || g<224 || g>228 || b<224 || b>228) return 0;
				}
			}
		}
	}
	else
	{
		for(_i=0; _i<16; _i++)
		{
			if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
			for(j=0; j<w; j++)
			{
				if(buf[(w*i+j)*ps+roffset]<253) return 0;
				if(((_i/8)+(j/8))%2==0)
				{
					if(buf[(w*i+j)*ps+goffset]<253) return 0;
					if(buf[(w*i+j)*ps+boffset]<253) return 0;
				}
				else
				{
					if(buf[(w*i+j)*ps+goffset]>2) return 0;
					if(buf[(w*i+j)*ps+boffset]>2) return 0;
				}
			}
		}
		for(_i=16; _i<h; _i++)
		{
			if(flags&TJ_BOTTOMUP) i=h-_i-1;  else i=_i;
			for(j=0; j<w; j++)
			{
				if(buf[(w*i+j)*ps+boffset]>2) return 0;
				if(((_i/8)+(j/8))%2==0)
				{
					if(buf[(w*i+j)*ps+roffset]>2) return 0;
					if(buf[(w*i+j)*ps+goffset]>2) return 0;
				}
				else
				{
					if(buf[(w*i+j)*ps+roffset]<253) return 0;
					if(buf[(w*i+j)*ps+goffset]<253) return 0;
				}
			}
		}
	}
	return 1;
}

#define checkval(v, cv) { \
	if(v<cv-1 || v>cv+1) { \
		printf("\nComp. %s at %d,%d should be %d, not %d\n", #v, i, j, cv, v); \
		retval=0;  goto bailout; \
	}}

#define checkval0(v) { \
	if(v>1) { \
		printf("\nComp. %s at %d,%d should be 0, not %d\n", #v, i, j, v); \
		retval=0;  goto bailout; \
	}}

#define checkval255(v) { \
	if(v<254 && !(v==217 && i==0 && j==21)) { \
		printf("\nComp. %s at %d,%d should be 255, not %d\n", #v, i, j, v); \
		retval=0;  goto bailout; \
	}}

#define PAD(v, p) ((v+(p)-1)&(~((p)-1)))

int checkbufyuv(unsigned char *buf, int w, int h, int subsamp)
{
	int i, j;
	int hsf=_hsf[subsamp], vsf=_vsf[subsamp];
	int pw=PAD(w, hsf), ph=PAD(h, vsf);
	int cw=pw/hsf, ch=ph/vsf;
	int ypitch=PAD(pw, 4), uvpitch=PAD(cw, 4);
	int retval=1;

	for(i=0; i<16; i++)
	{
		for(j=0; j<pw; j++)
		{
			unsigned char y=buf[ypitch*i+j];
			if(((i/8)+(j/8))%2==0) checkval255(y)
			else checkval(y, 76)
		}
	}
	for(i=16; i<ph; i++)
	{
		for(j=0; j<pw; j++)
		{
			unsigned char y=buf[ypitch*i+j];
			if(((i/8)+(j/8))%2==0) checkval0(y)
			else checkval(y, 226)
		}
	}
	if(subsamp!=TJ_GRAYSCALE)
	{
		for(i=0; i<16/vsf; i++)
		{
			for(j=0; j<cw; j++)
			{
				unsigned char u=buf[ypitch*ph + (uvpitch*i+j)],
					v=buf[ypitch*ph + uvpitch*ch + (uvpitch*i+j)];
				if(((i*vsf/8)+(j*hsf/8))%2==0)
				{
					checkval(u, 128);  checkval(v, 128);
				}
				else
				{
					checkval(u, 85);  checkval255(v);
				}
			}
		}
		for(i=16/vsf; i<ch; i++)
		{
			for(j=0; j<cw; j++)
			{
				unsigned char u=buf[ypitch*ph + (uvpitch*i+j)],
					v=buf[ypitch*ph + uvpitch*ch + (uvpitch*i+j)];
				if(((i*vsf/8)+(j*hsf/8))%2==0)
				{
					checkval(u, 128);  checkval(v, 128);
				}
				else
				{
					checkval0(u);  checkval(v, 149);
				}
			}
		}
	}

	bailout:
	if(retval==0)
	{
		for(i=0; i<ph; i++)
		{
			for(j=0; j<pw; j++)
				printf("%.3d ", buf[ypitch*i+j]);
			printf("\n");
		}
		printf("\n");
		for(i=0; i<ch; i++)
		{
			for(j=0; j<cw; j++)
				printf("%.3d ", buf[ypitch*ph + (uvpitch*i+j)]);
			printf("\n");
		}
		printf("\n");
		for(i=0; i<ch; i++)
		{
			for(j=0; j<cw; j++)
				printf("%.3d ", buf[ypitch*ph + uvpitch*ch + (uvpitch*i+j)]);
			printf("\n");
		}
		printf("\n");
	}

	return retval;
}

void writejpeg(unsigned char *jpegbuf, unsigned long jpgbufsize, char *filename)
{
	FILE *outfile=NULL;
	if((outfile=fopen(filename, "wb"))==NULL)
	{
		printf("ERROR: Could not open %s for writing.\n", filename);
		bailout();
	}
	if(fwrite(jpegbuf, jpgbufsize, 1, outfile)!=1)
	{
		printf("ERROR: Could not write to %s.\n", filename);
		bailout();
	}

	finally:
	if(outfile) fclose(outfile);
}

void gentestjpeg(tjhandle hnd, unsigned char *jpegbuf, unsigned long *size,
	int w, int h, int ps, char *basefilename, int subsamp, int qual, int flags)
{
	char tempstr[1024];  unsigned char *bmpbuf=NULL;
	const char *pixformat;  double t;

	if(flags&TJ_BGR)
	{
		if(ps==3) pixformat="BGR";
		else {if(flags&TJ_ALPHAFIRST) pixformat="XBGR";  else pixformat="BGRX";}
	}
	else
	{
		if(ps==3) pixformat="RGB";
		else {if(flags&TJ_ALPHAFIRST) pixformat="XRGB";  else pixformat="RGBX";}
	}
	if(ps==1) pixformat="Grayscale";
	if(yuv==YUVENCODE)
		printf("%s %s -> %s YUV ... ", pixformat,
			(flags&TJ_BOTTOMUP)?"Bottom-Up":"Top-Down ", _subnamel[subsamp]);
	else
		printf("%s %s -> %s Q%d ... ", pixformat,
			(flags&TJ_BOTTOMUP)?"Bottom-Up":"Top-Down ", _subnamel[subsamp], qual);

	if((bmpbuf=(unsigned char *)malloc(w*h*ps+1))==NULL)
	{
		printf("ERROR: Could not allocate buffer\n");  bailout();
	}
	initbuf(bmpbuf, w, h, ps, flags);
	memset(jpegbuf, 0,
		yuv==YUVENCODE? TJBUFSIZEYUV(w, h, subsamp):TJBUFSIZE(w, h));

	t=rrtime();
	if(yuv==YUVENCODE)
	{
		_catch(tjEncodeYUV(hnd, bmpbuf, w, 0, h, ps, jpegbuf, subsamp, flags));
		*size=TJBUFSIZEYUV(w, h, subsamp);
	}
	else
	{
		_catch(tjCompress(hnd, bmpbuf, w, 0, h, ps, jpegbuf, size, subsamp, qual,
			flags));
	}
	t=rrtime()-t;

	if(yuv==YUVENCODE)
		sprintf(tempstr, "%s_enc_%s_%s_%s.yuv", basefilename, pixformat,
			(flags&TJ_BOTTOMUP)? "BU":"TD", _subnames[subsamp]);
	else
		sprintf(tempstr, "%s_enc_%s_%s_%sQ%d.jpg", basefilename, pixformat,
			(flags&TJ_BOTTOMUP)? "BU":"TD", _subnames[subsamp], qual);
	writejpeg(jpegbuf, *size, tempstr);
	if(yuv==YUVENCODE)
	{
		if(checkbufyuv(jpegbuf, w, h, subsamp)) printf("Passed.");
		else {printf("FAILED!");  exitstatus=-1;}
	}
	else printf("Done.");
	printf("  %f ms\n  Result in %s\n", t*1000., tempstr);

	finally:
	if(bmpbuf) free(bmpbuf);
}

void gentestbmp(tjhandle hnd, unsigned char *jpegbuf, unsigned long jpegsize,
	int w, int h, int ps, char *basefilename, int subsamp, int flags)
{
	unsigned char *bmpbuf=NULL;
	const char *pixformat;  int _w=0, _h=0;  double t;
	unsigned long size=0;

	if(yuv==YUVENCODE) return;

	if(flags&TJ_BGR)
	{
		if(ps==3) pixformat="BGR";
		else {if(flags&TJ_ALPHAFIRST) pixformat="XBGR";  else pixformat="BGRX";}
	}
	else
	{
		if(ps==3) pixformat="RGB";
		else {if(flags&TJ_ALPHAFIRST) pixformat="XRGB";  else pixformat="RGBX";}
	}
	if(ps==1) pixformat="Grayscale";
	if(yuv==YUVDECODE)
		printf("JPEG -> YUV %s ... ", _subnames[subsamp]);
	else
		printf("JPEG -> %s %s ... ", pixformat,
			(flags&TJ_BOTTOMUP)?"Bottom-Up":"Top-Down ");

	_catch(tjDecompressHeader(hnd, jpegbuf, jpegsize, &_w, &_h));
	if(_w!=w || _h!=h)
	{
		printf("Incorrect JPEG header\n");  bailout();
	}

	if(yuv==YUVDECODE) size=TJBUFSIZEYUV(w, h, subsamp);
	else size=w*h*ps+1;
	if((bmpbuf=(unsigned char *)malloc(size))==NULL)
	{
		printf("ERROR: Could not allocate buffer\n");  bailout();
	}
	memset(bmpbuf, 0, size);

	t=rrtime();
	if(yuv==YUVDECODE)
	{
		_catch(tjDecompressToYUV(hnd, jpegbuf, jpegsize, bmpbuf, flags));
	}
	else
	{
		_catch(tjDecompress(hnd, jpegbuf, jpegsize, bmpbuf, w, w*ps, h, ps,
			flags));
	}
	t=rrtime()-t;

	if(yuv==YUVDECODE)
	{
		if(checkbufyuv(bmpbuf, w, h, subsamp)) printf("Passed.");
		else {printf("FAILED!");  exitstatus=-1;}
	}
	else
	{
		if(checkbuf(bmpbuf, w, h, ps, subsamp, flags)) printf("Passed.");
		else
		{
			printf("FAILED!");  exitstatus=-1;
			dumpbuf(bmpbuf, w, h, ps, flags);
		}
	}
	printf("  %f ms\n\n", t*1000.);

	finally:
	if(bmpbuf) free(bmpbuf);
}

void dotest(int w, int h, int ps, int subsamp, char *basefilename)
{
	tjhandle hnd=NULL, dhnd=NULL;  unsigned char *jpegbuf=NULL;
	unsigned long size;

	size=(yuv==YUVENCODE? TJBUFSIZEYUV(w, h, subsamp):TJBUFSIZE(w, h));
	if((jpegbuf=(unsigned char *)malloc(size)) == NULL)
	{
		puts("ERROR: Could not allocate buffer.");  bailout();
	}

	if((hnd=tjInitCompress())==NULL)
		{printf("Error in tjInitCompress():\n%s\n", tjGetErrorStr());  bailout();}
	if((dhnd=tjInitDecompress())==NULL)
		{printf("Error in tjInitDecompress():\n%s\n", tjGetErrorStr());  bailout();}

	gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, 0);
	gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, 0);

	if(ps==1 || yuv==YUVDECODE) goto finally;

	gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_BGR);
	gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_BGR);

	gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_BOTTOMUP);
	gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_BOTTOMUP);

	gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_BGR|TJ_BOTTOMUP);
	gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_BGR|TJ_BOTTOMUP);

	if(ps==4)
	{
		gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_ALPHAFIRST);
		gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_ALPHAFIRST);

		gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_ALPHAFIRST|TJ_BGR);
		gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_ALPHAFIRST|TJ_BGR);

		gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_ALPHAFIRST|TJ_BOTTOMUP);
		gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_ALPHAFIRST|TJ_BOTTOMUP);

		gentestjpeg(hnd, jpegbuf, &size, w, h, ps, basefilename, subsamp, 100, TJ_ALPHAFIRST|TJ_BGR|TJ_BOTTOMUP);
		gentestbmp(dhnd, jpegbuf, size, w, h, ps, basefilename, subsamp, TJ_ALPHAFIRST|TJ_BGR|TJ_BOTTOMUP);
	}

	finally:
	if(hnd) tjDestroy(hnd);
	if(dhnd) tjDestroy(dhnd);

	if(jpegbuf) free(jpegbuf);
}

#define MAXLENGTH 2048

void dotest1(void)
{
	int i, j, i2;  unsigned char *bmpbuf=NULL, *jpgbuf=NULL;
	tjhandle hnd=NULL;  unsigned long size;
	if((hnd=tjInitCompress())==NULL)
		{printf("Error in tjInitCompress():\n%s\n", tjGetErrorStr());  bailout();}
	printf("Buffer size regression test\n");
	for(j=1; j<48; j++)
	{
		for(i=1; i<(j==1?MAXLENGTH:48); i++)
		{
			if(i%100==0) printf("%.4d x %.4d\b\b\b\b\b\b\b\b\b\b\b", i, j);
			if((bmpbuf=(unsigned char *)malloc(i*j*4))==NULL
			|| (jpgbuf=(unsigned char *)malloc(TJBUFSIZE(i, j)))==NULL)
			{
				printf("Memory allocation failure\n");  bailout();
			}
			memset(bmpbuf, 0, i*j*4);
			for(i2=0; i2<i*j; i2++)
			{
				bmpbuf[i2*4]=pixels[i2%9][2];
				bmpbuf[i2*4+1]=pixels[i2%9][1];
				bmpbuf[i2*4+2]=pixels[i2%9][0];
			}
			_catch(tjCompress(hnd, bmpbuf, i, 0, j, 4,
				jpgbuf, &size, TJ_444, 100, TJ_BGR));
			free(bmpbuf);  bmpbuf=NULL;  free(jpgbuf);  jpgbuf=NULL;

			if((bmpbuf=(unsigned char *)malloc(j*i*4))==NULL
			|| (jpgbuf=(unsigned char *)malloc(TJBUFSIZE(j, i)))==NULL)
			{
				printf("Memory allocation failure\n");  bailout();
			}
			for(i2=0; i2<j*i; i2++)
			{
				if(i2%2==0) bmpbuf[i2*4]=bmpbuf[i2*4+1]=bmpbuf[i2*4+2]=0xFF;
				else bmpbuf[i2*4]=bmpbuf[i2*4+1]=bmpbuf[i2*4+2]=0;
			}
			_catch(tjCompress(hnd, bmpbuf, j, 0, i, 4,
				jpgbuf, &size, TJ_444, 100, TJ_BGR));
			free(bmpbuf);  bmpbuf=NULL;  free(jpgbuf);  jpgbuf=NULL;
		}
	}
	printf("Done.      \n");

	finally:
	if(bmpbuf) free(bmpbuf);  if(jpgbuf) free(jpgbuf);
	if(hnd) tjDestroy(hnd);
}

int main(int argc, char *argv[])
{
	int doyuv=0;
	if(argc>1 && !stricmp(argv[1], "-yuv")) doyuv=1;
	if(doyuv) yuv=YUVENCODE;
	dotest(35, 39, 3, TJ_444, "test");
	dotest(39, 41, 4, TJ_444, "test");
	if(doyuv)
	{
		dotest(41, 35, 3, TJ_422, "test");
		dotest(35, 39, 4, TJ_422, "test");
		dotest(39, 41, 3, TJ_420, "test");
		dotest(41, 35, 4, TJ_420, "test");
	}
	dotest(35, 39, 1, TJ_GRAYSCALE, "test");
	dotest(39, 41, 3, TJ_GRAYSCALE, "test");
	dotest(41, 35, 4, TJ_GRAYSCALE, "test");
	if(!doyuv) dotest1();
	if(doyuv)
	{
		yuv=YUVDECODE;
		dotest(48, 48, 3, TJ_444, "test");
		dotest(35, 39, 3, TJ_444, "test");
		dotest(48, 48, 3, TJ_422, "test");
		dotest(39, 41, 3, TJ_422, "test");
		dotest(48, 48, 3, TJ_420, "test");
		dotest(41, 35, 3, TJ_420, "test");
		dotest(48, 48, 3, TJ_GRAYSCALE, "test");
		dotest(35, 39, 3, TJ_GRAYSCALE, "test");
		dotest(48, 48, 1, TJ_GRAYSCALE, "test");
		dotest(39, 41, 1, TJ_GRAYSCALE, "test");
	}

	return exitstatus;
}
