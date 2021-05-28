/* SumatraPDF: support JPEG-XR images using built-in windows codec */

#include "mupdf/fitz.h"

#ifndef _WIN32
#error "only supported under windows"
#endif

#define COBJMACROS
#include <wincodec.h>

static fz_pixmap* fz_load_jxr_or_info(fz_context* ctx, const unsigned char* data, size_t size, int* wp, int* hp,
																		int* xresp, int* yresp, fz_colorspace** cspacep) {
	int info_only = wp && hp && xresp && yresp && cspacep;
	fz_pixmap* pix = NULL;
	IStream* stream = NULL;
	IWICImagingFactory* factory = NULL;
	IWICBitmapDecoder* decoder = NULL;
    IWICFormatConverter* converter = NULL;
	IWICBitmapFrameDecode* src_frame = NULL;
	IWICBitmapSource* src_bitmap = NULL;
	int codec_available = 0;
	LARGE_INTEGER zero = {0};
	UINT width, height;
	double xres, yres;
	ULONG written;
	HRESULT hr;

	hr = CoInitialize(NULL);
	if (FAILED(hr))
			fz_throw(ctx, FZ_ERROR_GENERIC, "JPEG-XR codec is not available");
	hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_ALL, &IID_IWICImagingFactory, (void**)&factory);
	if (FAILED(hr))
		goto CleanUp;
	hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
	if (FAILED(hr))
		goto CleanUp;
	hr = IStream_Write(stream, data, (ULONG)size, &written);
	if (FAILED(hr))
		goto CleanUp;
	hr = IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
	if (FAILED(hr))
		goto CleanUp;
	hr = IWICImagingFactory_CreateDecoderFromStream(factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder);
	if (FAILED(hr))
		goto CleanUp;
	hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
	if (FAILED(hr))
		goto CleanUp;
	hr = IWICBitmapDecoder_GetFrame(decoder, 0, &src_frame);
	if (FAILED(hr))
		goto CleanUp;
	hr = IUnknown_QueryInterface(src_frame, &IID_IWICBitmapSource, &src_bitmap);
	if (FAILED(hr))
		goto CleanUp;
	hr = IWICFormatConverter_Initialize(converter, src_bitmap, &GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,	NULL, 0.f, WICBitmapPaletteTypeCustom);
	if (FAILED(hr))
		goto CleanUp;
	hr = IWICFormatConverter_GetSize(converter, &width, &height);
	if (FAILED(hr))
		goto CleanUp;
	hr = IWICFormatConverter_GetResolution(converter, &xres, &yres);
	if (FAILED(hr))
		goto CleanUp;
	codec_available = 1;

	if (info_only) {
		*cspacep = fz_device_bgr(ctx);
		*wp = width;
		*hp = height;
		*xresp = (int)(xres + 0.5);
		*yresp = (int)(yres + 0.5);
	} else {
		fz_try(ctx) {
			pix = fz_new_pixmap(ctx, fz_device_bgr(ctx), width, height, NULL, 1);
		}
		fz_catch(ctx) {
			pix = NULL;
			goto CleanUp;
		}
		hr = IWICFormatConverter_CopyPixels(converter, NULL, pix->w * pix->n, pix->w * pix->h * pix->n, pix->samples);
		if (FAILED(hr)) {
			fz_drop_pixmap(ctx, pix);
			pix = NULL;
			goto CleanUp;
		}
		pix->xres = (int)(xres + 0.5);
		pix->yres = (int)(yres + 0.5);
	}

CleanUp:
	if (src_bitmap)
		IUnknown_Release(src_bitmap);
	if (converter)
		IUnknown_Release(converter);
	if (src_frame)
		IUnknown_Release(src_frame);
	if (decoder)
		IUnknown_Release(decoder);
	if (factory)
		IUnknown_Release(factory);
	if (stream)
		IUnknown_Release(stream);

	if (codec_available) {
		if (!pix && !info_only)
			fz_throw(ctx, FZ_ERROR_GENERIC, "JPEG-XR codec failed to decode the image");
		return pix;
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "JPEG-XR codec is not available");
}

fz_pixmap* fz_load_jxr(fz_context* ctx, const unsigned char* data, size_t size) {
	return fz_load_jxr_or_info(ctx, data, size, NULL, NULL, NULL, NULL, NULL);
}

void fz_load_jxr_info(fz_context* ctx, const unsigned char* data, size_t size, int* wp, int* hp, int* xresp, int* yresp,
										fz_colorspace** cspacep) {
	fz_load_jxr_or_info(ctx, data, size, wp, hp, xresp, yresp, cspacep);
	if (*cspacep) {
		/* *cspacep is a borrowed device colorspace */
		*cspacep = fz_keep_colorspace(ctx, *cspacep);
	}
}
