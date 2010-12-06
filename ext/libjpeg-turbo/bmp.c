/* Copyright (C)2004 Landmark Graphics Corporation
 * Copyright (C)2005 Sun Microsystems, Inc.
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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
 #include <io.h>
#else
 #include <unistd.h>
#endif
#include "./rrutil.h"
#include "./bmp.h"

#ifndef BI_BITFIELDS
#define BI_BITFIELDS 3L
#endif
#ifndef BI_RGB
#define BI_RGB 0L
#endif

#define BMPHDRSIZE 54
typedef struct _bmphdr
{
	unsigned short bfType;
	unsigned int bfSize;
	unsigned short bfReserved1, bfReserved2;
	unsigned int bfOffBits;

	unsigned int biSize;
	int biWidth, biHeight;
	unsigned short biPlanes, biBitCount;
	unsigned int biCompression, biSizeImage;
	int biXPelsPerMeter, biYPelsPerMeter;
	unsigned int biClrUsed, biClrImportant;
} bmphdr;

static const char *__bmperr="No error";

static const int ps[BMPPIXELFORMATS]={3, 4, 3, 4, 4, 4};
static const int roffset[BMPPIXELFORMATS]={0, 0, 2, 2, 3, 1};
static const int goffset[BMPPIXELFORMATS]={1, 1, 1, 1, 2, 2};
static const int boffset[BMPPIXELFORMATS]={2, 2, 0, 0, 1, 3};

#define _throw(m) {__bmperr=m;  retcode=-1;  goto finally;}
#define _unix(f) {if((f)==-1) _throw(strerror(errno));}
#define _catch(f) {if((f)==-1) {retcode=-1;  goto finally;}}

#define readme(fd, addr, size) \
	if((bytesread=read(fd, addr, (size)))==-1) _throw(strerror(errno)); \
	if(bytesread!=(size)) _throw("Read error");

void pixelconvert(unsigned char *srcbuf, enum BMPPIXELFORMAT srcformat,
	int srcpitch, unsigned char *dstbuf, enum BMPPIXELFORMAT dstformat, int dstpitch,
	int w, int h, int flip)
{
	unsigned char *srcptr, *srcptr0, *dstptr, *dstptr0;
	int i, j;

	srcptr=flip? &srcbuf[srcpitch*(h-1)]:srcbuf;
	for(j=0, dstptr=dstbuf; j<h; j++,
		srcptr+=flip? -srcpitch:srcpitch, dstptr+=dstpitch)
	{
		for(i=0, srcptr0=srcptr, dstptr0=dstptr; i<w; i++,
			srcptr0+=ps[srcformat], dstptr0+=ps[dstformat])
		{
			dstptr0[roffset[dstformat]]=srcptr0[roffset[srcformat]];
			dstptr0[goffset[dstformat]]=srcptr0[goffset[srcformat]];
			dstptr0[boffset[dstformat]]=srcptr0[boffset[srcformat]];
		}
	}
}

int loadppm(int *fd, unsigned char **buf, int *w, int *h,
	enum BMPPIXELFORMAT f, int align, int dstbottomup, int ascii)
{
	FILE *fs=NULL;  int retcode=0, scalefactor, dstpitch;
	unsigned char *tempbuf=NULL;  char temps[255], temps2[255];
	int numread=0, totalread=0, pixel[3], i, j;

	if((fs=fdopen(*fd, "r"))==NULL) _throw(strerror(errno));

	do
	{
		if(!fgets(temps, 255, fs)) _throw("Read error");
		if(strlen(temps)==0 || temps[0]=='\n') continue;
		if(sscanf(temps, "%s", temps2)==1 && temps2[1]=='#') continue;
		switch(totalread)
		{
			case 0:
				if((numread=sscanf(temps, "%d %d %d", w, h, &scalefactor))==EOF)
					_throw("Read error");
				break;
			case 1:
				if((numread=sscanf(temps, "%d %d", h, &scalefactor))==EOF)
					_throw("Read error");
				break;
			case 2:
				if((numread=sscanf(temps, "%d", &scalefactor))==EOF)
					_throw("Read error");
				break;
		}
		totalread+=numread;
	} while(totalread<3);
	if((*w)<1 || (*h)<1 || scalefactor<1) _throw("Corrupt PPM header");

	dstpitch=(((*w)*ps[f])+(align-1))&(~(align-1));
	if((*buf=(unsigned char *)malloc(dstpitch*(*h)))==NULL)
		_throw("Memory allocation error");
	if(ascii)
	{
		for(j=0; j<*h; j++)
		{
			for(i=0; i<*w; i++)
			{
				if(fscanf(fs, "%d%d%d", &pixel[0], &pixel[1], &pixel[2])!=3)
					_throw("Read error");
				(*buf)[j*dstpitch+i*ps[f]+roffset[f]]=(unsigned char)(pixel[0]*255/scalefactor);
				(*buf)[j*dstpitch+i*ps[f]+goffset[f]]=(unsigned char)(pixel[1]*255/scalefactor);
				(*buf)[j*dstpitch+i*ps[f]+boffset[f]]=(unsigned char)(pixel[2]*255/scalefactor);
			}
		}
	}
	else
	{
		if(scalefactor!=255)
			_throw("Binary PPMs must have 8-bit components");
		if((tempbuf=(unsigned char *)malloc((*w)*(*h)*3))==NULL)
			_throw("Memory allocation error");
		if(fread(tempbuf, (*w)*(*h)*3, 1, fs)!=1) _throw("Read error");
		pixelconvert(tempbuf, BMP_RGB, (*w)*3, *buf, f, dstpitch, *w, *h, dstbottomup);
	}

	finally:
	if(fs) {fclose(fs);  *fd=-1;}
	if(tempbuf) free(tempbuf);
	return retcode;
}


int loadbmp(char *filename, unsigned char **buf, int *w, int *h, 
	enum BMPPIXELFORMAT f, int align, int dstbottomup)
{
	int fd=-1, bytesread, srcpitch, srcbottomup=1, srcps, dstpitch,
		retcode=0;
	unsigned char *tempbuf=NULL;
	bmphdr bh;  int flags=O_RDONLY;

	dstbottomup=dstbottomup? 1:0;
	#ifdef _WIN32
	flags|=O_BINARY;
	#endif
	if(!filename || !buf || !w || !h || f<0 || f>BMPPIXELFORMATS-1 || align<1)
		_throw("invalid argument to loadbmp()");
	if((align&(align-1))!=0)
		_throw("Alignment must be a power of 2");
	_unix(fd=open(filename, flags));

	readme(fd, &bh.bfType, sizeof(unsigned short));
	if(!littleendian())	bh.bfType=byteswap16(bh.bfType);

	if(bh.bfType==0x3650)
	{
		_catch(loadppm(&fd, buf, w, h, f, align, dstbottomup, 0));
		goto finally;
	}
	if(bh.bfType==0x3350)
	{
		_catch(loadppm(&fd, buf, w, h, f, align, dstbottomup, 1));
		goto finally;
	}

	readme(fd, &bh.bfSize, sizeof(unsigned int));
	readme(fd, &bh.bfReserved1, sizeof(unsigned short));
	readme(fd, &bh.bfReserved2, sizeof(unsigned short));
	readme(fd, &bh.bfOffBits, sizeof(unsigned int));
	readme(fd, &bh.biSize, sizeof(unsigned int));
	readme(fd, &bh.biWidth, sizeof(int));
	readme(fd, &bh.biHeight, sizeof(int));
	readme(fd, &bh.biPlanes, sizeof(unsigned short));
	readme(fd, &bh.biBitCount, sizeof(unsigned short));
	readme(fd, &bh.biCompression, sizeof(unsigned int));
	readme(fd, &bh.biSizeImage, sizeof(unsigned int));
	readme(fd, &bh.biXPelsPerMeter, sizeof(int));
	readme(fd, &bh.biYPelsPerMeter, sizeof(int));
	readme(fd, &bh.biClrUsed, sizeof(unsigned int));
	readme(fd, &bh.biClrImportant, sizeof(unsigned int));

	if(!littleendian())
	{
		bh.bfSize=byteswap(bh.bfSize);
		bh.bfOffBits=byteswap(bh.bfOffBits);
		bh.biSize=byteswap(bh.biSize);
		bh.biWidth=byteswap(bh.biWidth);
		bh.biHeight=byteswap(bh.biHeight);
		bh.biPlanes=byteswap16(bh.biPlanes);
		bh.biBitCount=byteswap16(bh.biBitCount);
		bh.biCompression=byteswap(bh.biCompression);
		bh.biSizeImage=byteswap(bh.biSizeImage);
		bh.biXPelsPerMeter=byteswap(bh.biXPelsPerMeter);
		bh.biYPelsPerMeter=byteswap(bh.biYPelsPerMeter);
		bh.biClrUsed=byteswap(bh.biClrUsed);
		bh.biClrImportant=byteswap(bh.biClrImportant);
	}

	if(bh.bfType!=0x4d42 || bh.bfOffBits<BMPHDRSIZE
	|| bh.biWidth<1 || bh.biHeight==0)
		_throw("Corrupt bitmap header");
	if((bh.biBitCount!=24 && bh.biBitCount!=32) || bh.biCompression!=BI_RGB)
		_throw("Only uncompessed RGB bitmaps are supported");

	*w=bh.biWidth;  *h=bh.biHeight;  srcps=bh.biBitCount/8;
	if(*h<0) {*h=-(*h);  srcbottomup=0;}
	srcpitch=(((*w)*srcps)+3)&(~3);
	dstpitch=(((*w)*ps[f])+(align-1))&(~(align-1));

	if(srcpitch*(*h)+bh.bfOffBits!=bh.bfSize) _throw("Corrupt bitmap header");
	if((tempbuf=(unsigned char *)malloc(srcpitch*(*h)))==NULL
	|| (*buf=(unsigned char *)malloc(dstpitch*(*h)))==NULL)
		_throw("Memory allocation error");
	if(lseek(fd, (long)bh.bfOffBits, SEEK_SET)!=(long)bh.bfOffBits)
		_throw(strerror(errno));
	_unix(bytesread=read(fd, tempbuf, srcpitch*(*h)));
	if(bytesread!=srcpitch*(*h)) _throw("Read error");

	pixelconvert(tempbuf, BMP_BGR, srcpitch, *buf, f, dstpitch, *w, *h, 
		srcbottomup!=dstbottomup);

	finally:
	if(tempbuf) free(tempbuf);
	if(fd!=-1) close(fd);
	return retcode;
}

#define writeme(fd, addr, size) \
	if((byteswritten=write(fd, addr, (size)))==-1) _throw(strerror(errno)); \
	if(byteswritten!=(size)) _throw("Write error");

int saveppm(char *filename, unsigned char *buf, int w, int h,
	enum BMPPIXELFORMAT f, int srcpitch, int srcbottomup)
{
	FILE *fs=NULL;  int retcode=0;
	unsigned char *tempbuf=NULL;

	if((fs=fopen(filename, "wb"))==NULL) _throw(strerror(errno));
	if(fprintf(fs, "P6\n")<1) _throw("Write error");
	if(fprintf(fs, "%d %d\n", w, h)<1) _throw("Write error");
	if(fprintf(fs, "255\n")<1) _throw("Write error");

	if((tempbuf=(unsigned char *)malloc(w*h*3))==NULL)
		_throw("Memory allocation error");

	pixelconvert(buf, f, srcpitch, tempbuf, BMP_RGB, w*3, w, h, 
		srcbottomup);

	if((fwrite(tempbuf, w*h*3, 1, fs))!=1) _throw("Write error");

	finally:
	if(tempbuf) free(tempbuf);
	if(fs) fclose(fs);
	return retcode;
}

int savebmp(char *filename, unsigned char *buf, int w, int h,
	enum BMPPIXELFORMAT f, int srcpitch, int srcbottomup)
{
	int fd=-1, byteswritten, dstpitch, retcode=0;
	int flags=O_RDWR|O_CREAT|O_TRUNC;
	unsigned char *tempbuf=NULL;  char *temp;
	bmphdr bh;  int mode;

	#ifdef _WIN32
	flags|=O_BINARY;  mode=_S_IREAD|_S_IWRITE;
	#else
	mode=S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	#endif
	if(!filename || !buf || w<1 || h<1 || f<0 || f>BMPPIXELFORMATS-1 || srcpitch<0)
		_throw("bad argument to savebmp()");

	if(srcpitch==0) srcpitch=w*ps[f];

	if((temp=strrchr(filename, '.'))!=NULL)
	{
		if(!stricmp(temp, ".ppm"))
			return saveppm(filename, buf, w, h, f, srcpitch, srcbottomup);
	}

	_unix(fd=open(filename, flags, mode));
	dstpitch=((w*3)+3)&(~3);

	bh.bfType=0x4d42;
	bh.bfSize=BMPHDRSIZE+dstpitch*h;
	bh.bfReserved1=0;  bh.bfReserved2=0;
	bh.bfOffBits=BMPHDRSIZE;
	bh.biSize=40;
	bh.biWidth=w;  bh.biHeight=h;
	bh.biPlanes=0;  bh.biBitCount=24;
	bh.biCompression=BI_RGB;  bh.biSizeImage=0;
	bh.biXPelsPerMeter=0;  bh.biYPelsPerMeter=0;
	bh.biClrUsed=0;  bh.biClrImportant=0;

	if(!littleendian())
	{
		bh.bfType=byteswap16(bh.bfType);
		bh.bfSize=byteswap(bh.bfSize);
		bh.bfOffBits=byteswap(bh.bfOffBits);
		bh.biSize=byteswap(bh.biSize);
		bh.biWidth=byteswap(bh.biWidth);
		bh.biHeight=byteswap(bh.biHeight);
		bh.biPlanes=byteswap16(bh.biPlanes);
		bh.biBitCount=byteswap16(bh.biBitCount);
		bh.biCompression=byteswap(bh.biCompression);
		bh.biSizeImage=byteswap(bh.biSizeImage);
		bh.biXPelsPerMeter=byteswap(bh.biXPelsPerMeter);
		bh.biYPelsPerMeter=byteswap(bh.biYPelsPerMeter);
		bh.biClrUsed=byteswap(bh.biClrUsed);
		bh.biClrImportant=byteswap(bh.biClrImportant);
	}

	writeme(fd, &bh.bfType, sizeof(unsigned short));
	writeme(fd, &bh.bfSize, sizeof(unsigned int));
	writeme(fd, &bh.bfReserved1, sizeof(unsigned short));
	writeme(fd, &bh.bfReserved2, sizeof(unsigned short));
	writeme(fd, &bh.bfOffBits, sizeof(unsigned int));
	writeme(fd, &bh.biSize, sizeof(unsigned int));
	writeme(fd, &bh.biWidth, sizeof(int));
	writeme(fd, &bh.biHeight, sizeof(int));
	writeme(fd, &bh.biPlanes, sizeof(unsigned short));
	writeme(fd, &bh.biBitCount, sizeof(unsigned short));
	writeme(fd, &bh.biCompression, sizeof(unsigned int));
	writeme(fd, &bh.biSizeImage, sizeof(unsigned int));
	writeme(fd, &bh.biXPelsPerMeter, sizeof(int));
	writeme(fd, &bh.biYPelsPerMeter, sizeof(int));
	writeme(fd, &bh.biClrUsed, sizeof(unsigned int));
	writeme(fd, &bh.biClrImportant, sizeof(unsigned int));

	if((tempbuf=(unsigned char *)malloc(dstpitch*h))==NULL)
		_throw("Memory allocation error");

	pixelconvert(buf, f, srcpitch, tempbuf, BMP_BGR, dstpitch, w, h, 
		!srcbottomup);

	if((byteswritten=write(fd, tempbuf, dstpitch*h))!=dstpitch*h)
		_throw(strerror(errno));

	finally:
	if(tempbuf) free(tempbuf);
	if(fd!=-1) close(fd);
	return retcode;
}

const char *bmpgeterr(void)
{
	return __bmperr;
}
