/* SumatraPDF: support JPEG-XR images */

#include <windows.h>
#include <wincodec.h>
extern "C" {
#include "fitz-internal.h"
}

extern "C" fz_pixmap *
fz_load_jxr(fz_context *ctx, unsigned char *data, int size)
{
	fz_pixmap *pix = NULL;
	IStream *stream = NULL;
	IWICImagingFactory *pFactory = NULL;
	IWICBitmapDecoder *pDecoder = NULL;
	IWICFormatConverter *pConverter = NULL;
	IWICBitmapFrameDecode *srcFrame = NULL;
	UINT width, height;
	LARGE_INTEGER zero = { 0 };
	ULONG written;

	CoInitialize(NULL);
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_ALL, IID_IWICImagingFactory, (void **)&pFactory);
	if (FAILED(hr))
	{
		CoUninitialize();
		fz_throw(ctx, "JPEG-XR codec is not available");
	}

#define HR(hr) if (FAILED(hr)) goto CleanUp
	HR(CreateStreamOnHGlobal(NULL, TRUE, &stream));
	HR(stream->Write(data, (ULONG)size, &written));
	HR(stream->Seek(zero, STREAM_SEEK_SET, NULL));
	HR(pFactory->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder));
	HR(pDecoder->GetFrame(0, &srcFrame));
	HR(pFactory->CreateFormatConverter(&pConverter));
	HR(pConverter->Initialize(srcFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom));
	HR(pConverter->GetSize(&width, &height));
#undef HR

	fz_try(ctx)
	{
		pix = fz_new_pixmap(ctx, fz_device_bgr, width, height);
	}
	fz_catch(ctx)
	{
		pix = NULL;
		goto CleanUp;
	}
	hr = pConverter->CopyPixels(NULL, pix->w * pix->n, pix->w * pix->h * pix->n, pix->samples);
	if (FAILED(hr))
	{
		fz_drop_pixmap(ctx, pix);
		pix = NULL;
	}

CleanUp:
	if (pConverter) pConverter->Release();
	if (srcFrame) srcFrame->Release();
	if (pDecoder) pDecoder->Release();
	if (pFactory) pFactory->Release();
	if (stream) stream->Release();
	CoUninitialize();

	if (!pix)
		fz_throw(ctx, "JPEG-XR codec failed to decode the image");
	return pix;
}
