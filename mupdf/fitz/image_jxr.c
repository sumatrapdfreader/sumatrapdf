/* SumatraPDF: support JPEG-XR images */

#include "fitz-internal.h"
#ifdef _WIN32
#define COBJMACROS
#include <wincodec.h>
#endif

fz_pixmap *
fz_load_jxr(fz_context *ctx, unsigned char *data, int size)
{
#ifndef _WIN32
	fz_throw(ctx, "JPEG-XR codec is not available");
	return NULL;
#else
	fz_pixmap *pix = NULL;
	IStream *stream = NULL;
	IWICImagingFactory *factory = NULL;
	IWICBitmapDecoder *decoder = NULL;
	IWICFormatConverter *converter = NULL;
	IWICBitmapFrameDecode *src_frame = NULL;
	IWICBitmapSource *src_bitmap = NULL;
	int codec_available = 0;
	LARGE_INTEGER zero = { 0 };
	UINT width, height;
	ULONG written;
	HRESULT hr;

	CoInitialize(NULL);
#define HR(hr) if (FAILED(hr)) goto CleanUp
	HR(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_ALL, &IID_IWICImagingFactory, (void **)&factory));
	HR(CreateStreamOnHGlobal(NULL, TRUE, &stream));
	HR(IStream_Write(stream, data, (ULONG)size, &written));
	HR(IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL));
	HR(IWICImagingFactory_CreateDecoderFromStream(factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder));
	HR(IWICImagingFactory_CreateFormatConverter(factory, &converter));
	HR(IWICBitmapDecoder_GetFrame(decoder, 0, &src_frame));
	HR(IUnknown_QueryInterface(src_frame, &IID_IWICBitmapSource, &src_bitmap));
	HR(IWICFormatConverter_Initialize(converter, src_bitmap, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom));
	HR(IWICFormatConverter_GetSize(converter, &width, &height));
#undef HR
	codec_available = 1;

	fz_try(ctx)
	{
		pix = fz_new_pixmap(ctx, fz_device_bgr, width, height);
	}
	fz_catch(ctx)
	{
		pix = NULL;
		goto CleanUp;
	}
	hr = IWICFormatConverter_CopyPixels(converter, NULL, pix->w * pix->n, pix->w * pix->h * pix->n, pix->samples);
	if (FAILED(hr))
	{
		fz_drop_pixmap(ctx, pix);
		pix = NULL;
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
	CoUninitialize();

	if (!pix)
        fz_throw(ctx, codec_available ? "JPEG-XR codec failed to decode the image" : "JPEG-XR codec is not available");
	return pix;
#endif
}
