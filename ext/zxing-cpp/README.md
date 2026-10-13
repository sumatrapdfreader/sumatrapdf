[![Build Status](https://github.com/zxing-cpp/zxing-cpp/workflows/CI/badge.svg?branch=master)](https://github.com/zxing-cpp/zxing-cpp/actions?query=workflow%3ACI)

# ZXing-C++

ZXing-C++ ("zebra crossing") is an open-source, multi-format linear/matrix barcode image processing library implemented in C++.

It was originally ported from the Java [ZXing Library](https://github.com/zxing/zxing) but has been developed further and now includes many improvements in terms of runtime and detection performance. It can both read and write barcodes in a number of formats.

## Sponsors

You can sponsor this library at [GitHub Sponsors](https://github.com/sponsors/axxel).

Named Sponsors:
* [KURZ Digital Solutions GmbH & Co. KG](https://github.com/kurzdigital)
* [Useful Sensors Inc](https://github.com/usefulsensors)
* [EUREKAM](https://eurekam.fr/)

Thanks a lot for your contribution!

## Features

* Written in pure C++20 (/C++17), no third-party dependencies (for the library itself)
* Thread safe
* Wrappers/Bindings for:
  * [Android](wrappers/android/README.md)
  * [C](wrappers/c/README.md)
  * [iOS](wrappers/ios/README.md)
  * [Kotlin/Native](wrappers/kn/README.md)
  * [.NET](wrappers/dotnet/README.md)
  * [Python](wrappers/python/README.md)
  * [Rust](wrappers/rust/README.md)
  * [WebAssembly](wrappers/wasm/README.md)
  * [WinRT](wrappers/winrt/README.md)
  * [Flutter](https://pub.dev/packages/flutter_zxing) (external project)

## Supported Formats

| Linear product  | Linear industrial | Matrix             |
|-----------------|-------------------|--------------------|
| UPC-A           | Code 39           | QR Code            |
| UPC-E           | Code 93           | Micro QR Code      |
| EAN-8           | Code 128          | rMQR Code          |
| EAN-13          | Codabar           | Aztec              |
| DataBar         | DataBar Expanded  | DataMatrix         |
| DataBar Limited | DX Film Edge      | PDF417             |
|                 | ITF               | MaxiCode (partial) |

[Note:]
 * DataBar used to be called RSS.
 * DataBar, DX Film Edge, MaxiCode, Micro QR Code and rMQR Code are not supported for writing (unless the library is configured `ZXING_WRITERS=NEW` and `ZXING_EXPERIMENTAL_API=ON`).
 * Building with only C++17 (see [CMakeLists.txt](https://github.com/zxing-cpp/zxing-cpp/blob/d4b0f502775857f257d13efd25fb840ece1bca3e/CMakeLists.txt#L45)) changes the behavior of the library: it then lacks support for DataBarLimited and multi-symbol and position independent detection for DataMatrix.

## Getting Started

### To read barcodes:
1. Load your image into memory (3rd-party library required).
2. Call `ReadBarcodes()` from [`ReadBarcode.h`](core/src/ReadBarcode.h), the simplest API to get a list of `Barcode` objects.

A very simple example looks like this:
```c++
#include "ZXing/ReadBarcode.h"
#include <iostream>

int main(int argc, char** argv)
{
    int width, height;
    unsigned char* data;
    // load your image data from somewhere. ImageFormat::Lum assumes grey scale image data.

    auto image = ZXing::ImageView(data, width, height, ZXing::ImageFormat::Lum);
    auto options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::Any);
    auto barcodes = ZXing::ReadBarcodes(image, options);

    for (const auto& b : barcodes)
        std::cout << ZXing::ToString(b.format()) << ": " << b.text() << "\n";

    return 0;
}
```
To see the full capability of the API, have a look at [`ZXingReader.cpp`](example/ZXingReader.cpp).

[Note: At least C++17 is required on the client side to use the API.]

### To write barcodes:
1. Create a [`MultiFormatWriter`](core/src/MultiFormatWriter.h) instance with the format you want to generate. Set encoding and margins if needed.
2. Call `encode()` with text content and the image size. This returns a [`BitMatrix`](core/src/BitMatrix.h) which is a binary image of the barcode where `true` == visual black and `false` == visual white.
3. Convert the bit matrix to your native image format. See also the `ToMatrix<T>(BitMatrix&)` helper function.

As an example, have a look at [`ZXingWriter.cpp`](example/ZXingWriter.cpp). That file also contains example code showing the new `ZXING_EXPERIMENTAL_API` for writing barcodes.

## Web Demos
- [Read barcodes](https://zxing-cpp.github.io/zxing-cpp/demo_reader.html)
- [Write barcodes](https://zxing-cpp.github.io/zxing-cpp/demo_writer.html)
- [Read barcodes from camera](https://zxing-cpp.github.io/zxing-cpp/demo_cam_reader.html)

[Note: those live demos are not necessarily fully up-to-date at all times.]

## Build Instructions
These are the generic instructions to build the library on Windows/macOS/Linux. For details on how to build the individual wrappers, follow the links above.

1. Make sure [CMake](https://cmake.org) version 3.16 or newer is installed. The python module requires 3.18 or higher.
2. Make sure a sufficiently C++20 compliant compiler is installed (minimum VS 2019 16.10? / gcc 11 / clang 12?).
3. See the cmake `ZXING_...` options to enable the testing code, python wrapper, etc.

```
git clone https://github.com/zxing-cpp/zxing-cpp.git --recursive --single-branch --depth 1
cmake -S zxing-cpp -B zxing-cpp.release -DCMAKE_BUILD_TYPE=Release
cmake --build zxing-cpp.release -j8 --config Release
```

[Note: binary packages are available for/as
[vcpkg](https://github.com/Microsoft/vcpkg/tree/master/ports/nu-book-zxing-cpp),
[conan](https://github.com/conan-io/conan-center-index/tree/master/recipes/zxing-cpp),
[mingw](https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-zxing-cpp) and a bunch of
[linux distributions](https://repology.org/project/zxing-cpp/versions).]
