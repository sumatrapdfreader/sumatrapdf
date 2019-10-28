# source code for openjpeg apps:
add_subdirectory(common)
# Part 1 & 2:
add_subdirectory(jp2)

# optionals components:
if(BUILD_JPWL)
  add_subdirectory(jpwl)
endif()
if(BUILD_MJ2)
  add_subdirectory(mj2)
endif()
if(BUILD_JPIP)
  add_subdirectory(jpip)
endif()
if(BUILD_JP3D)
  add_subdirectory(jp3d)
endif()

# wx apps:
add_subdirectory(wx)
