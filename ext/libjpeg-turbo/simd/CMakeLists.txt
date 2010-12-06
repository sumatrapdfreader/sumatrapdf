if(NOT DEFINED NASM)
  set(NASM nasm)
endif()

if(SIMD_X86_64)
  set(NAFLAGS -fwin64 -DWIN64 -D__x86_64__ -I${CMAKE_SOURCE_DIR}/win/
    -I${CMAKE_CURRENT_SOURCE_DIR}/)
else()
  set(NAFLAGS -fwin32 -DWIN32 -I${CMAKE_SOURCE_DIR}/win/
    -I${CMAKE_CURRENT_SOURCE_DIR}/)
endif()

if(MSVC)
  set(NAFLAGS ${NAFLAGS} -DMSVC)
endif()

# This only works if building from the command line.  There is currently no way
# to set a variable's value based on the build type when using the MSVC IDE.
if(CMAKE_BUILD_TYPE STREQUAL "Debug"
  OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  set(NAFLAGS ${NAFLAGS} -g)
endif()

if(SIMD_X86_64)
  set(SIMD_BASENAMES jfsseflt-64 jccolss2-64 jdcolss2-64 jcsamss2-64
    jdsamss2-64 jdmerss2-64 jcqnts2i-64 jfss2fst-64 jfss2int-64 jiss2red-64
    jiss2int-64 jiss2fst-64 jcqnts2f-64 jiss2flt-64)
  message(STATUS "Building x86_64 SIMD extensions")
else()
  set(SIMD_BASENAMES jsimdcpu jccolmmx jdcolmmx jcsammmx jdsammmx jdmermmx
    jcqntmmx jfmmxfst jfmmxint jimmxred jimmxint jimmxfst jcqnt3dn jf3dnflt
    ji3dnflt jcqntsse jfsseflt jisseflt jccolss2 jdcolss2 jcsamss2 jdsamss2
    jdmerss2 jcqnts2i jfss2fst jfss2int jiss2red jiss2int jiss2fst jcqnts2f
    jiss2flt)
  message(STATUS "Building i386 SIMD extensions")
endif()

if(MSVC_IDE)
  set(OBJDIR "${CMAKE_CURRENT_BINARY_DIR}/$(OutDir)")
else()
  set(OBJDIR ${CMAKE_CURRENT_BINARY_DIR})
endif()

foreach(file ${SIMD_BASENAMES})
  set(DEPFILE "")
  set(SIMD_SRC ${CMAKE_CURRENT_SOURCE_DIR}/${file}.asm)
  if(${file} MATCHES col)
    set(DEPFILE ${SIMD_SRC})
    string(REGEX REPLACE "col" "clr" DEPFILE ${DEPFILE})
  endif()
  if(${file} MATCHES mer)
    set(DEPFILE ${SIMD_SRC})
    string(REGEX REPLACE "mer" "mrg" DEPFILE ${DEPFILE})
  endif()
  set(SIMD_OBJ ${OBJDIR}/${file}.obj)
  add_custom_command(OUTPUT ${SIMD_OBJ} DEPENDS ${SIMD_SRC} ${DEPFILE} *.inc
    COMMAND ${NASM} ${NAFLAGS} ${SIMD_SRC} -o${SIMD_OBJ})
  set(SIMD_OBJS ${SIMD_OBJS} ${SIMD_OBJ})
endforeach()

set(SIMD_OBJS ${SIMD_OBJS} PARENT_SCOPE)
add_custom_target(simd DEPENDS ${SIMD_OBJS})
