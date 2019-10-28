# Build the demo app, small examples

# First thing define the common source:
set(common_SRCS
  convert.c
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.c
  )

# Headers file are located here:
include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openjp3d
  ${LCMS_INCLUDE_DIRNAME}
  ${OPENJPEG_SOURCE_DIR}/src/bin/common
  ${Z_INCLUDE_DIRNAME}
  ${PNG_INCLUDE_DIRNAME}
  ${TIFF_INCLUDE_DIRNAME}
  )

if(WIN32)
  if(BUILD_SHARED_LIBS)
    add_definitions(-DOPJ_EXPORTS)
  else()
    add_definitions(-DOPJ_STATIC)
  endif()
endif()

# Loop over all executables:
foreach(exe opj_jp3d_compress opj_jp3d_decompress)
  add_executable(${exe} ${exe}.c ${common_SRCS})
  target_link_libraries(${exe} openjp3d)
  # On unix you need to link to the math library:
  if(UNIX)
    target_link_libraries(${exe} m)
  endif(UNIX)
  # Install exe
  install(TARGETS ${exe}
    EXPORT OpenJP3DTargets
    DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
  )
endforeach()
