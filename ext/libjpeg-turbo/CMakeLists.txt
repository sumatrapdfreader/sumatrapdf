#
# Setup
#

cmake_minimum_required(VERSION 2.6)

project(libjpeg-turbo C)
set(VERSION 1.2.1)

if(MINGW OR CYGWIN)
  execute_process(COMMAND "date" "+%Y%m%d" OUTPUT_VARIABLE BUILD)
  string(REGEX REPLACE "\n" "" BUILD ${BUILD})
elseif(WIN32)
  execute_process(COMMAND "${CMAKE_SOURCE_DIR}/cmakescripts/getdate.bat"
    OUTPUT_VARIABLE BUILD)
  string(REGEX REPLACE "\n" "" BUILD ${BUILD})
else()
  message(FATAL_ERROR "Platform not supported by this build system.  Use autotools instead.")
endif()

# This does nothing except when using MinGW.  CMAKE_BUILD_TYPE has no meaning
# in Visual Studio, and it always defaults to Debug when using NMake.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# This only works if building from the command line.  There is currently no way
# to set a variable's value based on the build type when using Visual Studio.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(BUILD "${BUILD}d")
endif()

message(STATUS "VERSION = ${VERSION}, BUILD = ${BUILD}")

option(WITH_SIMD "Include SIMD extensions" TRUE)
option(WITH_ARITH_ENC "Include arithmetic encoding support" TRUE)
option(WITH_ARITH_DEC "Include arithmetic decoding support" TRUE)
option(WITH_JPEG7 "Emulate libjpeg v7 API/ABI (this makes libjpeg-turbo backward incompatible with libjpeg v6b)" FALSE)
option(WITH_JPEG8 "Emulate libjpeg v8 API/ABI (this makes libjpeg-turbo backward incompatible with libjpeg v6b)" FALSE)
option(WITH_JAVA "Build Java wrapper for the TurboJPEG/OSS library" FALSE)

if(WITH_ARITH_ENC)
  set(C_ARITH_CODING_SUPPORTED 1)
  message(STATUS "Arithmetic encoding support enabled")
else()
  message(STATUS "Arithmetic encoding support disabled")
endif()

if(WITH_ARITH_DEC)
  set(D_ARITH_CODING_SUPPORTED 1)
  message(STATUS "Arithmetic decoding support enabled")
else()
  message(STATUS "Arithmetic decoding support disabled")
endif()

if(WITH_JAVA)
  message(STATUS "TurboJPEG/OSS Java wrapper enabled")
else()
  message(STATUS "TurboJPEG/OSS Java wrapper disabled")
endif()

set(JPEG_LIB_VERSION 62)
set(DLL_VERSION ${JPEG_LIB_VERSION})
set(FULLVERSION ${DLL_VERSION}.0.0)
if(WITH_JPEG8)
  set(JPEG_LIB_VERSION 80)
  set(DLL_VERSION 8)
  set(FULLVERSION ${DLL_VERSION}.0.2)
  message(STATUS "Emulating libjpeg v8 API/ABI")
elseif(WITH_JPEG7)
  set(JPEG_LIB_VERSION 70)
  set(DLL_VERSION 7)
  set(FULLVERSION ${DLL_VERSION}.0.0)
  message(STATUS "Emulating libjpeg v7 API/ABI")
endif(WITH_JPEG8)

if(MSVC)
  # Use the static C library for all build types
  foreach(var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    if(${var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${var} "${${var}}")
    endif()
  endforeach()

  add_definitions(-W3 -wd4996)
endif()

# Detect whether compiler is 64-bit
if(MSVC AND CMAKE_CL_64)
  set(SIMD_X86_64 1)
  set(64BIT 1)
elseif(CMAKE_SIZEOF_VOID_P MATCHES 8)
  set(SIMD_X86_64 1)
  set(64BIT 1)
endif()

if(64BIT)
  message(STATUS "64-bit build")
else()
  message(STATUS "32-bit build")
endif()

configure_file(win/jconfig.h.in jconfig.h)
configure_file(win/config.h.in config.h)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR})

if(WITH_JAVA)
  find_package(Java)
  find_package(JNI)
  if(DEFINED JAVACFLAGS)
    message(STATUS "Java compiler flags = ${JAVACFLAGS}")
  endif()
endif()


#
# Targets
#

set(JPEG_SOURCES jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c
  jcinit.c jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c jcphuff.c
  jcprepct.c jcsample.c jctrans.c jdapimin.c jdapistd.c jdatadst.c jdatasrc.c
  jdcoefct.c jdcolor.c jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c
  jdmaster.c jdmerge.c jdphuff.c jdpostct.c jdsample.c jdtrans.c jerror.c
  jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c jidctint.c jidctred.c
  jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c)

if(WITH_ARITH_ENC OR WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jaricom.c)
endif()

if(WITH_ARITH_ENC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jcarith.c)
endif()

if(WITH_ARITH_DEC)
  set(JPEG_SOURCES ${JPEG_SOURCES} jdarith.c)
endif()

if(WITH_SIMD)
  add_definitions(-DWITH_SIMD)
  add_subdirectory(simd)
  if(SIMD_X86_64)
    set(JPEG_SOURCES ${JPEG_SOURCES} simd/jsimd_x86_64.c)
  else()
    set(JPEG_SOURCES ${JPEG_SOURCES} simd/jsimd_i386.c)
  endif()
  # This tells CMake that the "source" files haven't been generated yet
  set_source_files_properties(${SIMD_OBJS} PROPERTIES GENERATED 1)
else()
  set(JPEG_SOURCES ${JPEG_SOURCES} jsimd_none.c)
  message(STATUS "Not using SIMD acceleration")
endif()

if(WITH_JAVA)
  add_subdirectory(java)
endif()

add_subdirectory(sharedlib)

add_library(jpeg-static STATIC ${JPEG_SOURCES} ${SIMD_OBJS})
if(NOT MSVC)
  set_target_properties(jpeg-static PROPERTIES OUTPUT_NAME jpeg)
endif()
if(WITH_SIMD)
  add_dependencies(jpeg-static simd)
endif()

set(TURBOJPEG_SOURCES turbojpeg.c transupp.c jdatadst-tj.c jdatasrc-tj.c)
if(WITH_JAVA)
  set(TURBOJPEG_SOURCES ${TURBOJPEG_SOURCES} turbojpeg-jni.c)
  include_directories(${JAVA_INCLUDE_PATH} ${JAVA_INCLUDE_PATH2})
endif()

add_library(turbojpeg SHARED ${TURBOJPEG_SOURCES})
set_target_properties(turbojpeg PROPERTIES DEFINE_SYMBOL DLLDEFINE)
if(MINGW)
  set_target_properties(turbojpeg PROPERTIES LINK_FLAGS -Wl,--kill-at)
endif()
target_link_libraries(turbojpeg jpeg-static)
set_target_properties(turbojpeg PROPERTIES LINK_INTERFACE_LIBRARIES "")

add_library(turbojpeg-static STATIC ${JPEG_SOURCES} ${SIMD_OBJS}
  turbojpeg.c transupp.c jdatadst-tj.c jdatasrc-tj.c)
if(NOT MSVC)
  set_target_properties(turbojpeg-static PROPERTIES OUTPUT_NAME turbojpeg)
endif()
if(WITH_SIMD)
  add_dependencies(turbojpeg-static simd)
endif()

add_executable(tjunittest tjunittest.c tjutil.c)
target_link_libraries(tjunittest turbojpeg)

add_executable(tjunittest-static tjunittest.c tjutil.c)
target_link_libraries(tjunittest-static turbojpeg-static)

add_executable(tjbench tjbench.c bmp.c tjutil.c rdbmp.c rdppm.c wrbmp.c
  wrppm.c)
target_link_libraries(tjbench turbojpeg jpeg-static)
set_property(TARGET tjbench PROPERTY COMPILE_FLAGS
  "-DBMP_SUPPORTED -DPPM_SUPPORTED")

add_executable(tjbench-static tjbench.c bmp.c tjutil.c rdbmp.c rdppm.c wrbmp.c
  wrppm.c)
target_link_libraries(tjbench-static turbojpeg-static jpeg-static)
set_property(TARGET tjbench-static PROPERTY COMPILE_FLAGS
  "-DBMP_SUPPORTED -DPPM_SUPPORTED")

add_executable(cjpeg-static cjpeg.c cdjpeg.c rdbmp.c rdgif.c rdppm.c rdswitch.c
  rdtarga.c)
set_property(TARGET cjpeg-static PROPERTY COMPILE_FLAGS
  "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED -DTARGA_SUPPORTED -DUSE_SETMODE")
target_link_libraries(cjpeg-static jpeg-static)

add_executable(djpeg-static djpeg.c cdjpeg.c rdcolmap.c rdswitch.c wrbmp.c wrgif.c
  wrppm.c wrtarga.c)
set_property(TARGET djpeg-static PROPERTY COMPILE_FLAGS
  "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED -DTARGA_SUPPORTED -DUSE_SETMODE")
target_link_libraries(djpeg-static jpeg-static)

add_executable(jpegtran-static jpegtran.c cdjpeg.c rdswitch.c transupp.c)
target_link_libraries(jpegtran-static jpeg-static)
set_property(TARGET jpegtran-static PROPERTY COMPILE_FLAGS "-DUSE_SETMODE")

add_executable(rdjpgcom rdjpgcom.c)

add_executable(wrjpgcom rdjpgcom.c)


#
# Tests
#

if(MSVC_IDE)
  set(OBJDIR "\${CTEST_CONFIGURATION_TYPE}/")
else()
  set(OBJDIR "")
endif()

enable_testing()
if(WITH_JAVA)
add_test(TJUnitTest ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR} TJUnitTest)
add_test(TJUnitTest-yuv ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR} TJUnitTest -yuv)
add_test(TJUnitTest-bi ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR} TJUnitTest -bi)
add_test(TJUnitTest-bi-yuv ${JAVA_RUNTIME} -cp java/${OBJDIR}turbojpeg.jar -Djava.library.path=${CMAKE_CURRENT_BINARY_DIR}/${OBJDIR} TJUnitTest -bi -yuv)
endif()
add_test(tjunittest tjunittest)
add_test(tjunittest-alloc tjunittest -alloc)
add_test(tjunittest-yuv tjunittest -yuv)
add_test(cjpeg-int sharedlib/cjpeg -dct int -outfile testoutint.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-int-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg testoutint.jpg)
add_test(cjpeg-fast sharedlib/cjpeg -dct fast -opt -outfile testoutfst.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-fast-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst.jpg testoutfst.jpg)
add_test(cjpeg-fast-100 sharedlib/cjpeg -dct fast -quality 100 -opt -outfile testoutfst100.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-fast-100-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst100.jpg testoutfst100.jpg)
add_test(cjpeg-float sharedlib/cjpeg -dct float -outfile testoutflt.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
if(WITH_SIMD)
add_test(cjpeg-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgflt.jpg testoutflt.jpg)
else()
add_test(cjpeg-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgflt-nosimd.jpg testoutflt.jpg)
endif()
add_test(cjpeg-int-gray sharedlib/cjpeg -dct int -grayscale -outfile testoutgray.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-int-gray-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimggray.jpg testoutgray.jpg)
add_test(djpeg-int sharedlib/djpeg -dct int -fast -ppm -outfile testoutint.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-int-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.ppm testoutint.ppm)
add_test(djpeg-fast sharedlib/djpeg -dct fast -ppm -outfile testoutfst.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-fast-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst.ppm testoutfst.ppm)
add_test(djpeg-float sharedlib/djpeg -dct float -ppm -outfile testoutflt.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
if(WITH_SIMD)
add_test(djpeg-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgflt.ppm testoutflt.ppm)
else()
add_test(djpeg-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm testoutflt.ppm)
endif()
add_test(djpeg-int-1_2 sharedlib/djpeg -dct int -scale 1/2 -ppm -outfile testoutint1_2.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-int-1_2-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint1_2.ppm testoutint1_2.ppm)
add_test(djpeg-fast-1_2 sharedlib/djpeg -dct fast -scale 1/2 -ppm -outfile testoutfst1_2.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-fast-1_2-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst1_2.ppm testoutfst1_2.ppm)
add_test(djpeg-int-1_4 sharedlib/djpeg -dct int -scale 1/4 -ppm -outfile testoutint1_4.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-int-1_4-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint1_4.ppm testoutint1_4.ppm)
add_test(djpeg-fast-1_4 sharedlib/djpeg -dct fast -scale 1/4 -ppm -outfile testoutfst1_4.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-fast-1_4-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst1_4.ppm testoutfst1_4.ppm)
add_test(djpeg-int-1_8 sharedlib/djpeg -dct int -scale 1/8 -ppm -outfile testoutint1_8.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-int-1_8-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint1_8.ppm testoutint1_8.ppm)
add_test(djpeg-fast-1_8 sharedlib/djpeg -dct fast -scale 1/8 -ppm -outfile testoutfst1_8.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-fast-1_8-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst1_8.ppm testoutfst1_8.ppm)
add_test(djpeg-256 sharedlib/djpeg -dct int -bmp -colors 256 -outfile testout.bmp  ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-256-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimg.bmp testout.bmp)
add_test(cjpeg-prog sharedlib/cjpeg -dct int -progressive -outfile testoutp.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-prog-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgp.jpg testoutp.jpg)
add_test(jpegtran-prog sharedlib/jpegtran -outfile testoutt.jpg testoutp.jpg)
add_test(jpegtran-prog-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg testoutt.jpg)
if(WITH_ARITH_ENC)
add_test(cjpeg-ari sharedlib/cjpeg -dct int -arithmetic -outfile testoutari.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-ari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg testoutari.jpg)
add_test(jpegtran-toari sharedlib/jpegtran -arithmetic -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg)
add_test(jpegtran-toari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg testouta.jpg)
endif()
if(WITH_ARITH_DEC)
add_test(djpeg-ari sharedlib/djpeg -dct int -fast -ppm -outfile testoutari.ppm ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(djpeg-ari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgari.ppm testoutari.ppm)
add_test(jpegtran-fromari	sharedlib/jpegtran -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(jpegtran-fromari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg testouta.jpg)
endif()
add_test(jpegtran-crop sharedlib/jpegtran -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(jpegtran-crop-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgcrop.jpg testoutcrop.jpg)

add_test(tjunittest-static tjunittest-static)
add_test(tjunittest-static-alloc tjunittest-static -alloc)
add_test(tjunittest-static-yuv tjunittest-static -yuv)
add_test(cjpeg-static-int cjpeg-static -dct int -outfile testoutint.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-int-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg testoutint.jpg)
add_test(cjpeg-static-fast cjpeg-static -dct fast -opt -outfile testoutfst.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-fast-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst.jpg testoutfst.jpg)
add_test(cjpeg-static-fast-100 cjpeg-static -dct fast -quality 100 -opt -outfile testoutfst100.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-fast-100-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst100.jpg testoutfst100.jpg)
add_test(cjpeg-static-float cjpeg-static -dct float -outfile testoutflt.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
if(WITH_SIMD)
add_test(cjpeg-static-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgflt.jpg testoutflt.jpg)
else()
add_test(cjpeg-static-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgflt-nosimd.jpg testoutflt.jpg)
endif()
add_test(cjpeg-static-int-gray cjpeg-static -dct int -grayscale -outfile testoutgray.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-int-gray-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimggray.jpg testoutgray.jpg)
add_test(djpeg-static-int djpeg-static -dct int -fast -ppm -outfile testoutint.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-int-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.ppm testoutint.ppm)
add_test(djpeg-static-fast djpeg-static -dct fast -ppm -outfile testoutfst.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-fast-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst.ppm testoutfst.ppm)
add_test(djpeg-static-float djpeg-static -dct float -ppm -outfile testoutflt.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
if(WITH_SIMD)
add_test(djpeg-static-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgflt.ppm testoutflt.ppm)
else()
add_test(djpeg-static-float-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm testoutflt.ppm)
endif()
add_test(djpeg-static-int-1_2 djpeg-static -dct int -scale 1/2 -ppm -outfile testoutint1_2.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-int-1_2-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint1_2.ppm testoutint1_2.ppm)
add_test(djpeg-static-fast-1_2 djpeg-static -dct fast -scale 1/2 -ppm -outfile testoutfst1_2.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-fast-1_2-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst1_2.ppm testoutfst1_2.ppm)
add_test(djpeg-static-int-1_4 djpeg-static -dct int -scale 1/4 -ppm -outfile testoutint1_4.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-int-1_4-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint1_4.ppm testoutint1_4.ppm)
add_test(djpeg-static-fast-1_4 djpeg-static -dct fast -scale 1/4 -ppm -outfile testoutfst1_4.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-fast-1_4-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst1_4.ppm testoutfst1_4.ppm)
add_test(djpeg-static-int-1_8 djpeg-static -dct int -scale 1/8 -ppm -outfile testoutint1_8.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-int-1_8-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint1_8.ppm testoutint1_8.ppm)
add_test(djpeg-static-fast-1_8 djpeg-static -dct fast -scale 1/8 -ppm -outfile testoutfst1_8.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-fast-1_8-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgfst1_8.ppm testoutfst1_8.ppm)
add_test(djpeg-static-256 djpeg-static -dct int -bmp -colors 256 -outfile testout.bmp  ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-256-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimg.bmp testout.bmp)
add_test(cjpeg-static-prog cjpeg-static -dct int -progressive -outfile testoutp.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-prog-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgp.jpg testoutp.jpg)
add_test(jpegtran-static-prog jpegtran-static -outfile testoutt.jpg testoutp.jpg)
add_test(jpegtran-static-prog-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg testoutt.jpg)
if(WITH_ARITH_ENC)
add_test(cjpeg-static-ari cjpeg-static -dct int -arithmetic -outfile testoutari.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-ari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg testoutari.jpg)
add_test(jpegtran-static-toari jpegtran-static -arithmetic -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg)
add_test(jpegtran-static-toari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg testouta.jpg)
endif()
if(WITH_ARITH_DEC)
add_test(djpeg-static-ari djpeg-static -dct int -fast -ppm -outfile testoutari.ppm ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(djpeg-static-ari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgari.ppm testoutari.ppm)
add_test(jpegtran-static-fromari	jpegtran-static -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(jpegtran-static-fromari-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg testouta.jpg)
endif()
add_test(jpegtran-static-crop jpegtran-static -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(jpegtran-static-crop-cmp ${CMAKE_COMMAND} -E compare_files ${CMAKE_SOURCE_DIR}/testimages/testimgcrop.jpg testoutcrop.jpg)

add_custom_target(testclean COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_SOURCE_DIR}/cmakescripts/testclean.cmake)


#
# Installer
#

if(MSVC)
  set(INST_PLATFORM "Visual C++")
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-vc)
  set(INST_DIR ${CMAKE_PROJECT_NAME})
elseif(MINGW)
  set(INST_PLATFORM GCC)
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-gcc)
  set(INST_DIR ${CMAKE_PROJECT_NAME}-gcc)
  set(INST_DEFS -DGCC)
endif()

if(64BIT)
  set(INST_PLATFORM "${INST_PLATFORM} 64-bit")
  set(INST_NAME ${INST_NAME}64)
  set(INST_DIR ${INST_DIR}64)
  set(INST_DEFS ${INST_DEFS} -DWIN64)
endif()

if(WITH_JAVA)
  set(INST_DEFS ${INST_DEFS} -DJAVA)
endif()

if(MSVC_IDE)
  set(INST_DEFS ${INST_DEFS} "-DBUILDDIR=${CMAKE_CFG_INTDIR}\\")
else()
  set(INST_DEFS ${INST_DEFS} "-DBUILDDIR=")
endif()

configure_file(release/libjpeg-turbo.nsi.in libjpeg-turbo.nsi @ONLY)

add_custom_target(installer
  makensis -nocd ${INST_DEFS} libjpeg-turbo.nsi
  DEPENDS jpeg jpeg-static turbojpeg turbojpeg-static rdjpgcom wrjpgcom
    cjpeg djpeg jpegtran tjbench
  SOURCES libjpeg-turbo.nsi)

install(TARGETS jpeg-static turbojpeg turbojpeg-static rdjpgcom wrjpgcom tjbench
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_SOURCE_DIR}/README ${CMAKE_SOURCE_DIR}/README-turbo.txt
  ${CMAKE_SOURCE_DIR}/example.c ${CMAKE_SOURCE_DIR}/libjpeg.txt 
  ${CMAKE_SOURCE_DIR}/structure.txt ${CMAKE_SOURCE_DIR}/usage.txt
  ${CMAKE_SOURCE_DIR}/wizard.txt
  DESTINATION doc)

install(FILES ${CMAKE_BINARY_DIR}/jconfig.h ${CMAKE_SOURCE_DIR}/jerror.h
  ${CMAKE_SOURCE_DIR}/jmorecfg.h ${CMAKE_SOURCE_DIR}/jpeglib.h
  ${CMAKE_SOURCE_DIR}/turbojpeg.h DESTINATION include)
