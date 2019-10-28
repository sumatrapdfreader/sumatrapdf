# jpwl apps

# First thing define the common source:
set(common_SRCS
  convert.c
  index.c
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/color.c
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.c
  )

# Headers file are located here:
include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h
  ${OPENJPEG_BINARY_DIR}/src/bin/common # opj_apps_config.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openmj2
  ${OPENJPEG_SOURCE_DIR}/src/bin/common
  ${LCMS_INCLUDE_DIRNAME}
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

add_definitions(-DOPJ_USE_LEGACY)
foreach(exe decompress compress)
  set(jpwl_exe opj_jpwl_${exe})
  set(jp2_exe opj_${exe})
  add_executable(${jpwl_exe}
    #../jp2/${jp2_exe}.c
    ${jpwl_exe}.c
    ${common_SRCS}
  )
  set_property(
    TARGET ${jpwl_exe}
    APPEND PROPERTY COMPILE_DEFINITIONS USE_JPWL
  )

  target_link_libraries(${jpwl_exe} openjpwl
    ${LCMS_LIBNAME} ${PNG_LIBNAME} ${TIFF_LIBNAME})

  # To support universal exe:
  if(ZLIB_FOUND AND APPLE)
    target_link_libraries(${jpwl_exe} z)
  else(ZLIB_FOUND AND APPLE)
    target_link_libraries(${jpwl_exe} ${Z_LIBNAME})
  endif()

  if(UNIX)
    target_link_libraries(${jpwl_exe} m)
  endif()

  install(TARGETS ${jpwl_exe}
    DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
  )
endforeach()
