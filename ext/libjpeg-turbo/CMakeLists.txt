#
# Setup
#

cmake_minimum_required(VERSION 2.6)

project(libjpeg-turbo C)
set(VERSION 1.3.0)

if(MINGW OR CYGWIN)
  execute_process(COMMAND "date" "+%Y%m%d" OUTPUT_VARIABLE BUILD)
  string(REGEX REPLACE "\n" "" BUILD ${BUILD})
elseif(WIN32)
  execute_process(COMMAND "wmic.exe" "os" "get" "LocalDateTime" OUTPUT_VARIABLE
    BUILD)
  string(REGEX REPLACE "[^0-9]" "" BUILD "${BUILD}")
  if (BUILD STREQUAL "")
    execute_process(COMMAND "cmd.exe" "/C" "DATE" "/T" OUTPUT_VARIABLE BUILD)
    string(REGEX REPLACE ".*[ ]([0-9]*)[/.]([0-9]*)[/.]([0-9]*).*" "\\3\\2\\1" BUILD "${BUILD}")
  else()
    string(SUBSTRING "${BUILD}" 0 8 BUILD)
  endif()
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
option(WITH_MEM_SRCDST "Include in-memory source/destination manager functions when emulating the libjpeg v6b or v7 API/ABI" TRUE)
option(WITH_JAVA "Build Java wrapper for the TurboJPEG library" FALSE)

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
  message(STATUS "TurboJPEG Java wrapper enabled")
else()
  message(STATUS "TurboJPEG Java wrapper disabled")
endif()

set(SO_AGE 0)
if(WITH_MEM_SRCDST)
  set(SO_AGE 1)
endif()

set(JPEG_LIB_VERSION 62)
set(DLL_VERSION ${JPEG_LIB_VERSION})
set(FULLVERSION ${DLL_VERSION}.${SO_AGE}.0)
if(WITH_JPEG8)
  set(JPEG_LIB_VERSION 80)
  set(DLL_VERSION 8)
  set(FULLVERSION ${DLL_VERSION}.0.2)
  message(STATUS "Emulating libjpeg v8 API/ABI")
elseif(WITH_JPEG7)
  set(JPEG_LIB_VERSION 70)
  set(DLL_VERSION 7)
  set(FULLVERSION ${DLL_VERSION}.${SO_AGE}.0)
  message(STATUS "Emulating libjpeg v7 API/ABI")
endif(WITH_JPEG8)

if(WITH_MEM_SRCDST)
  set(MEM_SRCDST_SUPPORTED 1)
  message(STATUS "In-memory source/destination managers enabled")
else()
  message(STATUS "In-memory source/destination managers disabled")
endif()

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

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  if(MSVC)
    set(CMAKE_INSTALL_PREFIX_DEFAULT ${CMAKE_PROJECT_NAME})
  else()
    set(CMAKE_INSTALL_PREFIX_DEFAULT ${CMAKE_PROJECT_NAME}-gcc)
  endif()
  if(64BIT)
    set(CMAKE_INSTALL_PREFIX_DEFAULT ${CMAKE_INSTALL_PREFIX_DEFAULT}64)
  endif()
  set(CMAKE_INSTALL_PREFIX "c:/${CMAKE_INSTALL_PREFIX_DEFAULT}" CACHE PATH
    "Directory into which to install libjpeg-turbo (default: c:/${CMAKE_INSTALL_PREFIX_DEFAULT})"
    FORCE)
endif()

message(STATUS "Install directory = ${CMAKE_INSTALL_PREFIX}")

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

set(MD5_JPEG_INT 9a68f56bc76e466aa7e52f415d0f4a5f)
set(MD5_JPEG_FAST 0e1502e7fa421835e376a314fac2a39f)
set(MD5_JPEG_FAST_100 7bf72a8e741d64eecb960c97323af77c)
set(MD5_JPEG_FLOAT d1623885ffafcd40c684af09e3d65cd5)
set(MD5_JPEG_FLOAT_NOSIMD fb4884c35f8273f498cb32879de5c455)
set(MD5_JPEG_INT_GRAY 72b51f894b8f4a10b3ee3066770aa38d)
set(MD5_PPM_INT d1ed0d11f076b842525271647716aeb8)
set(MD5_PPM_FAST 048298a2d2410261c0533cb97bcfef23)
set(MD5_PPM_FLOAT 7f5b446ee36b2630e06785b8d42af15f)
set(MD5_PPM_FLOAT_NOSIMD 64072f1dbdc5b3a187777788604971a5)
set(MD5_PPM_INT_2_1 9f9de8c0612f8d06869b960b05abf9c9)
set(MD5_PPM_INT_15_8 b6875bc070720b899566cc06459b63b7)
set(MD5_PPM_INT_7_4 06a177eae05f164fac57f7a2c346ee87)
set(MD5_PPM_INT_13_8 bc3452573c8152f6ae552939ee19f82f)
set(MD5_PPM_INT_3_2 f5a8b88a8a7f96016f04d259cf82ed67)
set(MD5_PPM_INT_11_8 d8cc73c0aaacd4556569b59437ba00a5)
set(MD5_PPM_INT_5_4 32775dd9ad2ab90f4c5b219b53e0c86c)
set(MD5_PPM_INT_9_8 d25e61bc7eac0002f5b393aa223747b6)
set(MD5_PPM_INT_7_8 ddb564b7c74a09494016d6cd7502a946)
set(MD5_PPM_INT_3_4 8ed8e68808c3fbc4ea764fc9d2968646)
set(MD5_PPM_INT_5_8 a3363274999da2366a024efae6d16c9b)
set(MD5_PPM_INT_1_2 e692a315cea26b988c8e8b29a5dbcd81)
set(MD5_PPM_INT_3_8 79eca9175652ced755155c90e785a996)
set(MD5_PPM_INT_1_4 79cd778f8bf1a117690052cacdd54eca)
set(MD5_PPM_INT_1_8 391b3d4aca640c8567d6f8745eb2142f)
set(MD5_PPM_FAST_1_2 f30bcf6d32ccd44cbdd9aeaacbd9454f)
set(MD5_BMP_256 4980185e3776e89bd931736e1cddeee6)
set(MD5_JPEG_ARI e986fb0a637a8d833d96e8a6d6d84ea1)
set(MD5_PPM_ARI 72b59a99bcf1de24c5b27d151bde2437)
set(MD5_JPEG_PROG 1c4afddc05c0a43489ee54438a482d92)
set(MD5_JPEG_CROP b4197f377e621c4e9b1d20471432610d)

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
add_test(cjpeg-int-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT} -DFILE=testoutint.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-fast sharedlib/cjpeg -dct fast -opt -outfile testoutfst.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-fast-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FAST} -DFILE=testoutfst.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-fast-100 sharedlib/cjpeg -dct fast -quality 100 -opt -outfile testoutfst100.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-fast-100-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FAST_100} -DFILE=testoutfst100.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-float sharedlib/cjpeg -dct float -outfile testoutflt.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
if(WITH_SIMD)
add_test(cjpeg-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FLOAT} -DFILE=testoutflt.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
else()
add_test(cjpeg-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FLOAT_NOSIMD} -DFILE=testoutflt.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
add_test(cjpeg-int-gray sharedlib/cjpeg -dct int -grayscale -outfile testoutgray.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-int-gray-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT_GRAY} -DFILE=testoutgray.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-int sharedlib/djpeg -dct int -fast -ppm -outfile testoutint.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-int-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_INT} -DFILE=testoutint.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-fast sharedlib/djpeg -dct fast -ppm -outfile testoutfst.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-fast-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FAST} -DFILE=testoutfst.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-float sharedlib/djpeg -dct float -ppm -outfile testoutflt.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
if(WITH_SIMD)
add_test(djpeg-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FLOAT} -DFILE=testoutflt.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
else()
add_test(djpeg-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FLOAT_NOSIMD} -DFILE=testoutflt.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
foreach(scale 2_1 15_8 7_4 13_8 3_2 11_8 5_4 9_8 7_8 3_4 5_8 1_2 3_8 1_4 1_8)
string(REGEX REPLACE "_" "/" scalearg ${scale})
add_test(djpeg-int-${scale} sharedlib/djpeg -dct int -nosmooth -scale ${scalearg} -ppm -outfile testoutint${scale}.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-int-${scale}-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_INT_${scale}} -DFILE=testoutint${scale}.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endforeach()
add_test(djpeg-fast-1_2 sharedlib/djpeg -dct fast -scale 1/2 -ppm -outfile testoutfst1_2.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-fast-1_2-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FAST_1_2} -DFILE=testoutfst1_2.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-256 sharedlib/djpeg -dct int -bmp -colors 256 -outfile testout.bmp  ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-256-cmp ${CMAKE_COMMAND} -DMD5=${MD5_BMP_256} -DFILE=testout.bmp -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-prog sharedlib/cjpeg -dct int -progressive -outfile testoutp.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-prog-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_PROG} -DFILE=testoutp.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(jpegtran-prog sharedlib/jpegtran -outfile testoutt.jpg testoutp.jpg)
add_test(jpegtran-prog-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT} -DFILE=testoutt.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
if(WITH_ARITH_ENC)
add_test(cjpeg-ari sharedlib/cjpeg -dct int -arithmetic -outfile testoutari.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-ari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_ARI} -DFILE=testoutari.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake )
add_test(jpegtran-toari sharedlib/jpegtran -arithmetic -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg)
add_test(jpegtran-toari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_ARI} -DFILE=testouta.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
if(WITH_ARITH_DEC)
add_test(djpeg-ari sharedlib/djpeg -dct int -fast -ppm -outfile testoutari.ppm ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(djpeg-ari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_ARI} -DFILE=testoutari.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(jpegtran-fromari	sharedlib/jpegtran -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(jpegtran-fromari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT} -DFILE=testouta.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
add_test(jpegtran-crop sharedlib/jpegtran -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(jpegtran-crop-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_CROP} -DFILE=testoutcrop.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)

add_test(tjunittest-static tjunittest-static)
add_test(tjunittest-static-alloc tjunittest-static -alloc)
add_test(tjunittest-static-yuv tjunittest-static -yuv)
add_test(cjpeg-static-int cjpeg-static -dct int -outfile testoutint.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-int-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT} -DFILE=testoutint.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-static-fast cjpeg-static -dct fast -opt -outfile testoutfst.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-fast-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FAST} -DFILE=testoutfst.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-static-fast-100 cjpeg-static -dct fast -quality 100 -opt -outfile testoutfst100.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-fast-100-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FAST_100} -DFILE=testoutfst100.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-static-float cjpeg-static -dct float -outfile testoutflt.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
if(WITH_SIMD)
add_test(cjpeg-static-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FLOAT} -DFILE=testoutflt.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
else()
add_test(cjpeg-static-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_FLOAT_NOSIMD} -DFILE=testoutflt.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
add_test(cjpeg-static-int-gray cjpeg-static -dct int -grayscale -outfile testoutgray.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-int-gray-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT_GRAY} -DFILE=testoutgray.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-static-int djpeg-static -dct int -fast -ppm -outfile testoutint.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-int-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_INT} -DFILE=testoutint.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-static-fast djpeg-static -dct fast -ppm -outfile testoutfst.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-fast-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FAST} -DFILE=testoutfst.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-static-float djpeg-static -dct float -ppm -outfile testoutflt.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
if(WITH_SIMD)
add_test(djpeg-static-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FLOAT} -DFILE=testoutflt.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
else()
add_test(djpeg-static-float-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FLOAT_NOSIMD} -DFILE=testoutflt.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
foreach(scale 2_1 15_8 7_4 13_8 3_2 11_8 5_4 9_8 7_8 3_4 5_8 1_2 3_8 1_4 1_8)
string(REGEX REPLACE "_" "/" scalearg ${scale})
add_test(djpeg-static-int-${scale} djpeg-static -dct int -nosmooth -scale ${scalearg} -ppm -outfile testoutint${scale}.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-int-${scale}-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_INT_${scale}} -DFILE=testoutint${scale}.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endforeach()
add_test(djpeg-static-fast-1_2 djpeg-static -dct fast -scale 1/2 -ppm -outfile testoutfst1_2.ppm ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-fast-1_2-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_FAST_1_2} -DFILE=testoutfst1_2.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(djpeg-static-256 djpeg-static -dct int -bmp -colors 256 -outfile testout.bmp  ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(djpeg-static-256-cmp ${CMAKE_COMMAND} -DMD5=${MD5_BMP_256} -DFILE=testout.bmp -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(cjpeg-static-prog cjpeg-static -dct int -progressive -outfile testoutp.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-prog-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_PROG} -DFILE=testoutp.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(jpegtran-static-prog jpegtran-static -outfile testoutt.jpg testoutp.jpg)
add_test(jpegtran-static-prog-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT} -DFILE=testoutt.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
if(WITH_ARITH_ENC)
add_test(cjpeg-static-ari cjpeg-static -dct int -arithmetic -outfile testoutari.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.ppm)
add_test(cjpeg-static-ari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_ARI} -DFILE=testoutari.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake )
add_test(jpegtran-static-toari jpegtran-static -arithmetic -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgint.jpg)
add_test(jpegtran-static-toari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_ARI} -DFILE=testouta.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
if(WITH_ARITH_DEC)
add_test(djpeg-static-ari djpeg-static -dct int -fast -ppm -outfile testoutari.ppm ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(djpeg-static-ari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_PPM_ARI} -DFILE=testoutari.ppm -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
add_test(jpegtran-static-fromari	jpegtran-static -outfile testouta.jpg ${CMAKE_SOURCE_DIR}/testimages/testimgari.jpg)
add_test(jpegtran-static-fromari-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_INT} -DFILE=testouta.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)
endif()
add_test(jpegtran-static-crop jpegtran-static -crop 120x90+20+50 -transpose -perfect -outfile testoutcrop.jpg ${CMAKE_SOURCE_DIR}/testimages/testorig.jpg)
add_test(jpegtran-static-crop-cmp ${CMAKE_COMMAND} -DMD5=${MD5_JPEG_CROP} -DFILE=testoutcrop.jpg -P ${CMAKE_SOURCE_DIR}/cmakescripts/md5cmp.cmake)

add_custom_target(testclean COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_SOURCE_DIR}/cmakescripts/testclean.cmake)


#
# Installer
#

if(MSVC)
  set(INST_PLATFORM "Visual C++")
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-vc)
  set(INST_REG_NAME ${CMAKE_PROJECT_NAME})
elseif(MINGW)
  set(INST_PLATFORM GCC)
  set(INST_NAME ${CMAKE_PROJECT_NAME}-${VERSION}-gcc)
  set(INST_REG_NAME ${CMAKE_PROJECT_NAME}-gcc)
  set(INST_DEFS -DGCC)
endif()

if(64BIT)
  set(INST_PLATFORM "${INST_PLATFORM} 64-bit")
  set(INST_NAME ${INST_NAME}64)
  set(INST_REG_NAME ${INST_DIR}64)
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

STRING(REGEX REPLACE "/" "\\\\" INST_DIR ${CMAKE_INSTALL_PREFIX}) 

configure_file(release/libjpeg-turbo.nsi.in libjpeg-turbo.nsi @ONLY)

if(WITH_JAVA)
  set(JAVA_DEPEND java)
endif()
add_custom_target(installer
  makensis -nocd ${INST_DEFS} libjpeg-turbo.nsi
  DEPENDS jpeg jpeg-static turbojpeg turbojpeg-static rdjpgcom wrjpgcom
    cjpeg djpeg jpegtran tjbench ${JAVA_DEPEND}
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
