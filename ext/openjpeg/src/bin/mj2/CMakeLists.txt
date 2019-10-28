# Makefile for the MJ2 codecs of the OpenJPEG library: frames_to_mj2, mj2_to_frames, extract_j2k_from_mj2 and wrap_j2k_in_mj2

set(common_SRCS ${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.c)

if(WIN32)
  if(BUILD_SHARED_LIBS)
    add_definitions(-DOPJ_EXPORTS)
  else()
    add_definitions(-DOPJ_STATIC)
  endif()
endif()

# Headers file are located here:
include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h
  ${OPENJPEG_BINARY_DIR}/src/bin/common # opj_apps_config.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openmj2
  ${OPENJPEG_SOURCE_DIR}/src/bin/common
  ${LCMS_INCLUDE_DIRNAME}
  )

foreach(exe
  opj_mj2_wrap
  opj_mj2_extract
  opj_mj2_decompress
  opj_mj2_compress
)
  add_definitions(-DOPJ_USE_LEGACY)
  add_executable(${exe}
    ${exe}.c
    ${common_SRCS}
    ${MJ2_SRCS}
    ${OPENJPEG_SOURCE_DIR}/src/bin/common/color.c
    )
  set_property(
    TARGET ${exe}
    APPEND PROPERTY COMPILE_DEFINITIONS USE_MJ2
  )
  target_link_libraries(${exe} ${LCMS_LIBNAME} openmj2)

  if(UNIX)
    target_link_libraries(${exe} m)
  endif()

  install(TARGETS ${exe}
  	DESTINATION ${OPENJPEG_INSTALL_BIN_DIR})
endforeach()
