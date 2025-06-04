# source code for openjpeg project:
# Part 1 & 2:
add_subdirectory(openjp2)

if(BUILD_JPIP)
  add_subdirectory(openjpip)
endif()
