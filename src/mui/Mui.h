/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct FrameRateWnd;
struct TxtNode;

namespace mui {

// using namespace Gdiplus;

using Gdiplus::ARGB;
using Gdiplus::Bitmap;
using Gdiplus::Brush;
using Gdiplus::Color;
using Gdiplus::CombineModeReplace;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::FontFamily;
using Gdiplus::FontStyle;
using Gdiplus::FontStyleBold;
using Gdiplus::FontStyleItalic;
using Gdiplus::FontStyleRegular;
using Gdiplus::FontStyleStrikeout;
using Gdiplus::FontStyleUnderline;
using Gdiplus::FrameDimensionPage;
using Gdiplus::FrameDimensionTime;
using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Image;
using Gdiplus::ImageAttributes;
using Gdiplus::InterpolationModeHighQualityBicubic;
using Gdiplus::LinearGradientBrush;
using Gdiplus::LinearGradientMode;
using Gdiplus::LinearGradientModeVertical;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::OutOfMemory;
using Gdiplus::Pen;
using Gdiplus::PenAlignmentInset;
using Gdiplus::PropertyItem;
using Gdiplus::Region;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringAlignmentCenter;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;
using Gdiplus::Win32Error;
using Gdiplus::WrapModeTileFlipXY;

#include "MuiBase.h"
#include "TextRender.h"

void Initialize();
void Destroy();

} // namespace mui
