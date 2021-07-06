# Generate target to build the html documentation through CMake tool
# After having configured the project with the BUILD_DOC option you can run make doc
# to generate the html documentation in the doc/html repository of the build folder.

# Try to find the doxygen tool
find_package(Doxygen)

if(DOXYGEN_FOUND)
  # Configure the doxygen config file with variable from CMake and move it
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.dox.cmake.in
    ${CMAKE_BINARY_DIR}/doc/Doxyfile-html.dox @ONLY)

  # Configure the html mainpage file of the doxygen documentation with variable
  # from CMake and move it
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mainpage.dox.in
    ${CMAKE_BINARY_DIR}/doc/mainpage.dox @ONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/openjpip.dox.in
    ${CMAKE_BINARY_DIR}/doc/openjpip.dox @ONLY)
  # copy png file to make local (binary tree) documentation valid:
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/jpip_architect.png
    ${CMAKE_BINARY_DIR}/doc/html/jpip_architect.png COPYONLY)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/jpip_protocol.png
    ${CMAKE_BINARY_DIR}/doc/html/jpip_protocol.png COPYONLY)

  file(GLOB headers
    ${OPENJPEG_SOURCE_DIR}/src/lib/openjp2/*.h
    ${OPENJPEG_SOURCE_DIR}/src/lib/openjp2/*.c
    ${OPENJPEG_SOURCE_DIR}/src/lib/openjpip/*.h
    ${OPENJPEG_SOURCE_DIR}/src/lib/openjpip/*.c
  )
  # Generate new target to build the html documentation
  add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/index.html
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/doc/Doxyfile-html.dox
    DEPENDS ${CMAKE_BINARY_DIR}/doc/Doxyfile-html.dox
            ${CMAKE_BINARY_DIR}/doc/mainpage.dox
            ${CMAKE_BINARY_DIR}/doc/openjpip.dox
            ${headers}
  )
  add_custom_target(doc ALL
    DEPENDS ${CMAKE_BINARY_DIR}/doc/html/index.html
    COMMENT "Building doxygen documentation"
  )

  # install HTML documentation (install png files too):
  install(DIRECTORY ${CMAKE_BINARY_DIR}/doc/html
    DESTINATION ${OPENJPEG_INSTALL_DOC_DIR}
    PATTERN ".svn" EXCLUDE
  )
else()
  message(STATUS "Doxygen not found, we cannot generate the documentation")
endif()
