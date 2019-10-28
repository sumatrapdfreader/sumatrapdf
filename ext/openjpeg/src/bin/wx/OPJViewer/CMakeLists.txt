project(viewer CXX)

find_package(wxWidgets REQUIRED)
include(${wxWidgets_USE_FILE})

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${OPENJPEG_SOURCE_DIR}/src/lib
  ${OPENJPEG_SOURCE_DIR}/src/bin
  ${OPENJPEG_SOURCE_DIR}/src/lib/openjp2
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h and opj_config_private.h
  )

# original flags:
# -DUSE_JPWL -DwxUSE_LIBOPENJPEG -DwxUSE_GUI=1 -DOPJ_STATIC -DOPJ_HTMLABOUT -DOPJ_INICONFIG -DUSE_JPSEC -DOPJ_MANYFORMATS
add_definitions(-DwxUSE_LIBOPENJPEG -DOPENJPEG_VERSION="1.5.0")
set(OPJV_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/source/imagjpeg2000.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/wxj2kparser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/OPJViewer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/wxjp2parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/OPJDialogs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/OPJThreads.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/source/OPJAbout.cpp
  ${OPENJPEG_SOURCE_DIR}/src/bin/jp2/index.c
  )
add_executable(opjviewer ${OPJV_SRCS})
target_link_libraries(opjviewer ${wxWidgets_LIBRARIES} openjpeg)
