# Needed to find libheif/heif_version.h while compiling the library
include_directories(${libheif_BINARY_DIR} ${libheif_SOURCE_DIR})

set (heif_convert_sources
  encoder.cc
  encoder.h
  encoder_y4m.cc
  encoder_y4m.h
  heif_convert.cc
)

set (additional_link_directories)
set (additional_libraries)
set (additional_includes)

include (${CMAKE_ROOT}/Modules/FindJPEG.cmake)

if(JPEG_FOUND)
add_definitions(-DHAVE_LIBJPEG=1)
include_directories(SYSTEM ${JPEG_INCLUDE_DIR})

include (${CMAKE_ROOT}/Modules/CheckCXXSourceCompiles.cmake)

set(CMAKE_REQUIRED_LIBRARIES ${JPEG_LIBRARIES})

# while the docs say JPEG_INCLUDE_DIRS, my FindJPEG.cmake script returns it in JPEG_INCLUDE_DIR
set(CMAKE_REQUIRED_INCLUDES ${JPEG_INCLUDE_DIRS} ${JPEG_INCLUDE_DIR})

check_cxx_source_compiles("
#include <stddef.h>
#include <stdio.h>
#include <jpeglib.h>

int main() {
  jpeg_write_icc_profile(NULL, NULL, 0);
  return 0;
}
" HAVE_JPEG_WRITE_ICC_PROFILE)
if(HAVE_JPEG_WRITE_ICC_PROFILE)
  add_definitions(-DHAVE_JPEG_WRITE_ICC_PROFILE=1)
endif()

set (heif_convert_sources
  ${heif_convert_sources}
  encoder_jpeg.cc
  encoder_jpeg.h
)
set (additional_libraries
  ${additional_libraries}
  ${JPEG_LIBRARIES}
)
set (additional_includes
  ${additional_includes}
  ${JPEG_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIR}
)
endif()

if(UNIX OR MINGW)
  include (${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)
  pkg_check_modules (LIBPNG libpng)
  if(LIBPNG_FOUND)
    add_definitions(-DHAVE_LIBPNG=1)
    set (heif_convert_sources
      ${heif_convert_sources}
      encoder_png.cc
      encoder_png.h
    )
    set (additional_link_directories
      ${additional_link_directories}
      ${LIBPNG_LIBRARY_DIRS}
    )
    set (additional_libraries
      ${additional_libraries}
      ${LIBPNG_LINK_LIBRARIES} ${LIBPNG_LIBRARIES}
    )
    set (additional_includes
      ${additional_includes}
      ${LIBPNG_INCLUDE_DIRS}
    )
  endif()
endif()

set (heif_info_sources
  heif_info.cc
)

set (heif_enc_sources
  heif_enc.cc
)

set (heif_test_sources
  heif_test.cc
)

if(MSVC)
  set (getopt_sources
    ../extra/getopt.c
    ../extra/getopt.h
    ../extra/getopt_long.c
  )
  include_directories ("../extra")
endif()

add_executable (heif-convert ${heif_convert_sources} ${getopt_sources})
target_link_directories (heif-convert PRIVATE ${additional_link_directories})
target_link_libraries (heif-convert heif ${additional_libraries})
target_include_directories(heif-convert PRIVATE ${additional_includes})
install(TARGETS heif-convert RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES heif-convert.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

add_executable (heif-info ${heif_info_sources} ${getopt_sources})
target_link_libraries (heif-info heif)
install(TARGETS heif-info RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES heif-info.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

add_executable (heif-enc ${heif_enc_sources} ${getopt_sources})
target_link_directories (heif-enc PRIVATE ${additional_link_directories})
target_link_libraries (heif-enc heif ${additional_libraries})
target_include_directories(heif-enc PRIVATE ${additional_includes})
install(TARGETS heif-enc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES heif-enc.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

add_executable (heif-test ${heif_test_sources} ${getopt_sources})
target_link_libraries (heif-test heif)


if(LIBPNG_FOUND)
  set (heif_thumbnailer_sources
    encoder.cc
    encoder.h
    heif_thumbnailer.cc
    encoder_png.cc
    encoder_png.h
    )

  add_executable (heif-thumbnailer ${heif_thumbnailer_sources})
  target_link_directories (heif-thumbnailer PRIVATE ${LIBPNG_LIBRARY_DIRS})
  target_link_libraries (heif-thumbnailer heif ${LIBPNG_LINK_LIBRARIES} ${LIBPNG_LIBRARIES})
  target_include_directories(heif-thumbnailer PRIVATE ${LIBPNG_INCLUDE_DIRS})
  install(TARGETS heif-thumbnailer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(FILES heif-thumbnailer.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
endif()
