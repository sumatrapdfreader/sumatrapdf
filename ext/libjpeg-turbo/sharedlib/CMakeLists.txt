# Anything that must be linked against the shared C library on Windows must
# be built in this subdirectory, because CMake doesn't allow us to override
# the compiler flags for each build type except at directory scope.  Note
# to CMake developers:  Add a COMPILE_FLAGS_<CONFIG> target property, or
# better yet, provide a friendly way of configuring a Windows target to use the
# static C library.

if(MSVC)
  # Build all configurations against shared C library
  foreach(var CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
    CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
    if(${var} MATCHES "/MT")
      string(REGEX REPLACE "/MT" "/MD" ${var} "${${var}}")
    endif()
  endforeach()
endif()

foreach(src ${JPEG_SOURCES})
  set(JPEG_SRCS ${JPEG_SRCS} ${CMAKE_SOURCE_DIR}/${src})
endforeach()

if(WITH_SIMD)
  # This tells CMake that the "source" files haven't been generated yet
  set_source_files_properties(${SIMD_OBJS} PROPERTIES GENERATED 1)
endif()

add_library(jpeg SHARED ${JPEG_SRCS} ${SIMD_OBJS}
  ${CMAKE_SOURCE_DIR}/win/jpeg${DLL_VERSION}.def)
set_target_properties(jpeg PROPERTIES SOVERSION ${DLL_VERSION}
  VERSION ${FULLVERSION})
if(MSVC)
  set_target_properties(jpeg PROPERTIES SUFFIX ${DLL_VERSION}.dll)
elseif(MINGW OR CYGWIN)
  set_target_properties(jpeg PROPERTIES SUFFIX -${DLL_VERSION}.dll)
endif(MSVC)
if(WITH_SIMD)
  add_dependencies(jpeg simd)
endif()

add_executable(cjpeg ../cjpeg.c ../cdjpeg.c ../rdbmp.c ../rdgif.c ../rdppm.c
  ../rdswitch.c ../rdtarga.c)
set_property(TARGET cjpeg PROPERTY COMPILE_FLAGS
  "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED -DTARGA_SUPPORTED -DUSE_SETMODE")
target_link_libraries(cjpeg jpeg)

add_executable(djpeg ../djpeg.c ../cdjpeg.c ../rdcolmap.c ../rdswitch.c
  ../wrbmp.c ../wrgif.c ../wrppm.c ../wrtarga.c)
set_property(TARGET djpeg PROPERTY COMPILE_FLAGS
  "-DBMP_SUPPORTED -DGIF_SUPPORTED -DPPM_SUPPORTED -DTARGA_SUPPORTED -DUSE_SETMODE")
target_link_libraries(djpeg jpeg)

add_executable(jpegtran ../jpegtran.c ../cdjpeg.c ../rdswitch.c ../transupp.c)
target_link_libraries(jpegtran jpeg)

install(TARGETS jpeg cjpeg djpeg jpegtran
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
