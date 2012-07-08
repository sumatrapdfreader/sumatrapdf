/* SumatraPDF: support JPEG-XR images */

#include "fitz-internal.h"
#ifdef _WIN32
#define COBJMACROS
#include <wincodec.h>
#endif

fz_pixmap *
fz_load_jxr(fz_context *ctx, unsigned char *data, int size)
{
#ifdef _WIN32
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
	double xres, yres;
	ULONG written;
	HRESULT hr;

	CoInitialize(NULL);
#define Check(hr) if (FAILED(hr)) goto CleanUp
	Check(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_ALL, &IID_IWICImagingFactory, (void **)&factory));
	Check(CreateStreamOnHGlobal(NULL, TRUE, &stream));
	Check(IStream_Write(stream, data, (ULONG)size, &written));
	Check(IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL));
	Check(IWICImagingFactory_CreateDecoderFromStream(factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder));
	Check(IWICImagingFactory_CreateFormatConverter(factory, &converter));
	Check(IWICBitmapDecoder_GetFrame(decoder, 0, &src_frame));
	Check(IUnknown_QueryInterface(src_frame, &IID_IWICBitmapSource, &src_bitmap));
	Check(IWICFormatConverter_Initialize(converter, src_bitmap, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom));
	Check(IWICFormatConverter_GetSize(converter, &width, &height));
	Check(IWICFormatConverter_GetResolution(converter, &xres, &yres));
#undef Check
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
		goto CleanUp;
	}
	pix->xres = (int)(xres + 0.5);
	pix->yres = (int)(yres + 0.5);

CleanUp:
#define Release(unk) if (unk) IUnknown_Release(unk)
	Release(src_bitmap);
	Release(converter);
	Release(src_frame);
	Release(decoder);
	Release(factory);
	Release(stream);
#undef Release
	CoUninitialize();

	if (codec_available)
	{
		if (!pix)
			fz_throw(ctx, "JPEG-XR codec failed to decode the image");
		return pix;
	}
#endif
	fz_throw(ctx, "JPEG-XR codec is not available");
	return NULL;
}
