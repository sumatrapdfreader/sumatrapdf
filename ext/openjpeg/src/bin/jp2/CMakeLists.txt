# Build the demo app, small examples

# First thing define the common source:
set(common_SRCS
  convert.c
  convert.h
  convertbmp.c
  index.c
  index.h
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/color.c
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/color.h
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.c
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_getopt.h
  ${OPENJPEG_SOURCE_DIR}/src/bin/common/opj_string.h
  )

if(OPJ_HAVE_LIBTIFF)
	list(APPEND common_SRCS converttif.c)
endif()
if(OPJ_HAVE_LIBPNG)
	list(APPEND common_SRCS convertpng.c)
endif()

# Headers file are located here:
include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h
  ${OPENJPEG_BINARY_DIR}/src/bin/common # opj_apps_config.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openjp2
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

# Loop over all executables:
foreach(exe opj_decompress opj_compress opj_dump)
  add_executable(${exe} ${exe}.c ${common_SRCS})
  if(${CMAKE_VERSION} VERSION_GREATER "2.8.11")
    target_compile_options(${exe} PRIVATE ${OPENJP2_COMPILE_OPTIONS})
  endif()
  target_link_libraries(${exe} ${OPENJPEG_LIBRARY_NAME}
    ${PNG_LIBNAME} ${TIFF_LIBNAME} ${LCMS_LIBNAME}
    )
  # To support universal exe:
  if(ZLIB_FOUND AND APPLE)
    target_link_libraries(${exe} z)
  else(ZLIB_FOUND AND APPLE)
    target_link_libraries(${exe} ${Z_LIBNAME})
  endif()

  # On unix you need to link to the math library:
  if(UNIX)
    target_link_libraries(${exe} m)
    IF("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
      target_link_libraries(${exe} rt)
    endif()
  endif()
  # Install exe
  install(TARGETS ${exe}
    EXPORT OpenJPEGTargets
    DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
  )
  if(OPJ_USE_DSYMUTIL)
    add_custom_command(TARGET ${exe} POST_BUILD
    COMMAND "dsymutil" "$<TARGET_FILE:${exe}>"
    COMMENT "dsymutil $<TARGET_FILE:${exe}>"
    DEPENDS ${exe})
  endif()
endforeach()

if(BUILD_DOC)
# Install man pages
install(
  FILES       ${OPENJPEG_SOURCE_DIR}/doc/man/man1/opj_compress.1
              ${OPENJPEG_SOURCE_DIR}/doc/man/man1/opj_decompress.1
              ${OPENJPEG_SOURCE_DIR}/doc/man/man1/opj_dump.1
  DESTINATION ${OPENJPEG_INSTALL_MAN_DIR}/man1)
#
endif()
