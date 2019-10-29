# Check for SIMD extensions.
include(CMakePushCheckState)

function(webp_check_compiler_flag WEBP_SIMD_FLAG ENABLE_SIMD)
  if(NOT ENABLE_SIMD)
    message(STATUS "Disabling ${WEBP_SIMD_FLAG} optimization.")
    set(WEBP_HAVE_${WEBP_SIMD_FLAG} 0 PARENT_SCOPE)
    return()
  endif()
  unset(WEBP_HAVE_FLAG_${WEBP_SIMD_FLAG} CACHE)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})
  check_c_source_compiles("
      #include \"${CMAKE_CURRENT_LIST_DIR}/../src/dsp/dsp.h\"
      int main(void) {
        #if !defined(WEBP_USE_${WEBP_SIMD_FLAG})
        this is not valid code
        #endif
        return 0;
      }
    " WEBP_HAVE_FLAG_${WEBP_SIMD_FLAG})
  cmake_pop_check_state()
  if(WEBP_HAVE_FLAG_${WEBP_SIMD_FLAG})
    set(WEBP_HAVE_${WEBP_SIMD_FLAG} 1 PARENT_SCOPE)
  else()
    set(WEBP_HAVE_${WEBP_SIMD_FLAG} 0 PARENT_SCOPE)
  endif()
endfunction()

# those are included in the names of WEBP_USE_* in c++ code.
set(WEBP_SIMD_FLAGS "SSE41;SSE2;MIPS32;MIPS_DSP_R2;NEON;MSA")
set(WEBP_SIMD_FILE_EXTENSIONS
    "_sse41.c;_sse2.c;_mips32.c;_mips_dsp_r2.c;_neon.c;_msa.c")
if(MSVC)
  # MSVC does not have a SSE4 flag but AVX support implies SSE4 support.
  set(SIMD_ENABLE_FLAGS "/arch:AVX;/arch:SSE2;;;;")
  set(SIMD_DISABLE_FLAGS)
else()
  set(SIMD_ENABLE_FLAGS
      "-msse4.1;-msse2;-mips32;-mdspr2;-mfpu=neon;-mmsa")
  set(SIMD_DISABLE_FLAGS
      "-mno-sse4.1;-mno-sse2;;-mno-dspr2;;-mno-msa")
endif()

set(WEBP_SIMD_FILES_TO_NOT_INCLUDE)
set(WEBP_SIMD_FILES_TO_INCLUDE)
set(WEBP_SIMD_FLAGS_TO_INCLUDE)

if(${ANDROID})
  if(${ANDROID_ABI} STREQUAL "armeabi-v7a")
    # This is because Android studio uses the configuration "-march=armv7-a
    # -mfloat-abi=softfp -mfpu=vfpv3-d16" that does not trigger neon
    # optimizations but should (as this configuration does not exist anymore).
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon ")
  endif()
endif()

list(LENGTH WEBP_SIMD_FLAGS WEBP_SIMD_FLAGS_LENGTH)
math(EXPR WEBP_SIMD_FLAGS_RANGE "${WEBP_SIMD_FLAGS_LENGTH} - 1")
unset(HIGHEST_SSE_FLAG)

foreach(I_SIMD RANGE ${WEBP_SIMD_FLAGS_RANGE})
  list(GET WEBP_SIMD_FLAGS ${I_SIMD} WEBP_SIMD_FLAG)

  # First try with no extra flag added as the compiler might have default flags
  # (especially on Android).
  unset(WEBP_HAVE_${WEBP_SIMD_FLAG} CACHE)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_FLAGS)
  webp_check_compiler_flag(${WEBP_SIMD_FLAG} ${WEBP_ENABLE_SIMD})
  if(NOT WEBP_HAVE_${WEBP_SIMD_FLAG})
    list(GET SIMD_ENABLE_FLAGS ${I_SIMD} SIMD_COMPILE_FLAG)
    set(CMAKE_REQUIRED_FLAGS ${SIMD_COMPILE_FLAG})
    webp_check_compiler_flag(${WEBP_SIMD_FLAG} ${WEBP_ENABLE_SIMD})
  else()
    if(MSVC)
      list(GET SIMD_ENABLE_FLAGS ${I_SIMD} SIMD_COMPILE_FLAG)
    else()
      set(SIMD_COMPILE_FLAG " ")
    endif()
  endif()
  # Check which files we should include or not.
  list(GET WEBP_SIMD_FILE_EXTENSIONS ${I_SIMD} WEBP_SIMD_FILE_EXTENSION)
  file(GLOB SIMD_FILES "${CMAKE_CURRENT_LIST_DIR}/../"
            "src/dsp/*${WEBP_SIMD_FILE_EXTENSION}")
  if(WEBP_HAVE_${WEBP_SIMD_FLAG})
    if(${I_SIMD} LESS 2 AND NOT HIGHEST_SSE_FLAG)
      set(HIGHEST_SSE_FLAG ${SIMD_COMPILE_FLAG})
    endif()
    # Memorize the file and flags.
    foreach(FILE ${SIMD_FILES})
      list(APPEND WEBP_SIMD_FILES_TO_INCLUDE ${FILE})
      if(${I_SIMD} LESS 2)
        list(APPEND WEBP_SIMD_FLAGS_TO_INCLUDE ${HIGHEST_SSE_FLAG})
      else()
        list(APPEND WEBP_SIMD_FLAGS_TO_INCLUDE ${SIMD_COMPILE_FLAG})
      endif()
    endforeach()
  else()
    # Remove the file from the list.
    foreach(FILE ${SIMD_FILES})
      list(APPEND WEBP_SIMD_FILES_NOT_TO_INCLUDE ${FILE})
    endforeach()
    # Explicitly disable SIMD.
    if(SIMD_DISABLE_FLAGS)
      list(GET SIMD_DISABLE_FLAGS ${I_SIMD} SIMD_COMPILE_FLAG)
      include(CheckCCompilerFlag)
      if(SIMD_COMPILE_FLAG)
        unset(HAS_COMPILE_FLAG CACHE)
        check_c_compiler_flag(${SIMD_COMPILE_FLAG} HAS_COMPILE_FLAG)
        if(HAS_COMPILE_FLAG)
          # Do one more check for Clang to circumvent CMake issue 13194.
          if(COMMAND check_compiler_flag_common_patterns)
            # Only in CMake 3.0 and above.
            check_compiler_flag_common_patterns(COMMON_PATTERNS)
          else()
            set(COMMON_PATTERNS)
          endif()
          set(CMAKE_REQUIRED_DEFINITIONS ${SIMD_COMPILE_FLAG})
          check_c_source_compiles("int main(void) {return 0;}"
                                  FLAG_${SIMD_COMPILE_FLAG}
                                  FAIL_REGEX
                                  "warning: argument unused during compilation:"
                                  ${COMMON_PATTERNS})
          if(NOT FLAG_${SIMD_COMPILE_FLAG})
            unset(HAS_COMPILE_FLAG CACHE)
          endif()
        endif()
        if(HAS_COMPILE_FLAG)
          set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SIMD_COMPILE_FLAG}")
        endif()
      endif()
    endif()
  endif()
  cmake_pop_check_state()
endforeach()
