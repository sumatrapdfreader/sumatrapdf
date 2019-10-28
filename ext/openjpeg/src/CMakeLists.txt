# required dep for server:
#if(BUILD_JPIP_SERVER)
#  find_package(CURL REQUIRED)
#  find_package(FCGI REQUIRED)
#  find_package(Threads REQUIRED)
#  if(NOT CMAKE_USE_PTHREADS_INIT)
#    message(FATAL_ERROR "Only pthread are supported")
#  endif()
#endif()

#add_subdirectory(lib)
#add_subdirectory(bin)
