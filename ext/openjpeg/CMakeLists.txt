include_regular_expression("^.*$")

#
install( FILES  ${CMAKE_CURRENT_BINARY_DIR}/opj_config.h
 DESTINATION ${OPENJPEG_INSTALL_INCLUDE_DIR} COMPONENT Headers)

include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h and opj_config_private.h
)
# Defines the source code for the library
set(OPENJPEG_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/bio.c
  ${CMAKE_CURRENT_SOURCE_DIR}/cio.c
  ${CMAKE_CURRENT_SOURCE_DIR}/dwt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/event.c
  ${CMAKE_CURRENT_SOURCE_DIR}/image.c
  ${CMAKE_CURRENT_SOURCE_DIR}/invert.c
  ${CMAKE_CURRENT_SOURCE_DIR}/j2k.c
  ${CMAKE_CURRENT_SOURCE_DIR}/jp2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mct.c
  ${CMAKE_CURRENT_SOURCE_DIR}/mqc.c
  ${CMAKE_CURRENT_SOURCE_DIR}/openjpeg.c
  ${CMAKE_CURRENT_SOURCE_DIR}/opj_clock.c
  ${CMAKE_CURRENT_SOURCE_DIR}/pi.c
  ${CMAKE_CURRENT_SOURCE_DIR}/raw.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t1.c
  ${CMAKE_CURRENT_SOURCE_DIR}/t2.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tcd.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tgt.c
  ${CMAKE_CURRENT_SOURCE_DIR}/function_list.c
)
if(BUILD_JPIP)
  add_definitions(-DUSE_JPIP)
  set(OPENJPEG_SRCS
    ${OPENJPEG_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/cidx_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/phix_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ppix_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thix_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/tpix_manager.c
  )
endif()

# Build the library
if(WIN32)
  if(BUILD_SHARED_LIBS)
    add_definitions(-DOPJ_EXPORTS)
  else()
    add_definitions(-DOPJ_STATIC)
  endif()
endif()
add_library(${OPENJPEG_LIBRARY_NAME} ${OPENJPEG_SRCS})
if(UNIX)
  target_link_libraries(${OPENJPEG_LIBRARY_NAME} m)
endif()
set_target_properties(${OPENJPEG_LIBRARY_NAME} PROPERTIES ${OPENJPEG_LIBRARY_PROPERTIES})

# Install library
install(TARGETS ${OPENJPEG_LIBRARY_NAME}
  EXPORT OpenJPEGTargets
  RUNTIME DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
  LIBRARY DESTINATION ${OPENJPEG_INSTALL_LIB_DIR} COMPONENT Libraries
  ARCHIVE DESTINATION ${OPENJPEG_INSTALL_LIB_DIR} COMPONENT Libraries
)

# Install includes files
install(FILES openjpeg.h opj_stdint.h
  DESTINATION ${OPENJPEG_INSTALL_INCLUDE_DIR} COMPONENT Headers
)

if(BUILD_DOC)
# install man page of the library
install(
  FILES       ${OPENJPEG_SOURCE_DIR}/doc/man/man3/libopenjp2.3
  DESTINATION ${OPENJPEG_INSTALL_MAN_DIR}/man3)
endif()

# internal utilities to generate t1_luts.h (part of the jp2 lib)
# no need to install:
add_executable(t1_generate_luts t1_generate_luts.c)
if(UNIX)
  target_link_libraries(t1_generate_luts m)
endif()

# Experimental option; let's how cppcheck performs
# Implementation details:
# I could not figure out how to easily upload a file to CDash. Instead simply
# pretend cppcheck is part of the Build step. Technically cppcheck can even
# output gcc formatted error/warning report
# Another implementation detail: I could not redirect error to the error
# catching mechanism something is busted in cmake 2.8.5, I had to use the
# warning regex to catch them.
if(OPENJPEG_CPPCHECK)
  find_package(CPPCHECK REQUIRED)
  foreach(f ${OPENJPEG_SRCS})
    # cppcheck complains about too many configuration, pretend to be WIN32:
    add_custom_command(TARGET ${OPENJPEG_LIBRARY_NAME}
      COMMAND ${CPPCHECK_EXECUTABLE} -DWIN32 ${f})
  endforeach()
endif()
