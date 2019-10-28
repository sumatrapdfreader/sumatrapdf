# Headers file are located here:
include_directories(
  ${OPENJPEG_BINARY_DIR}/src/lib/openjp2 # opj_config.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openjp2
  ${OPENJPEG_SOURCE_DIR}/src/bin/common # opj_getopt.h
  ${OPENJPEG_SOURCE_DIR}/src/bin/jp2 # convert.h
  ${OPENJPEG_SOURCE_DIR}/src/lib/openjpip
  ${FCGI_INCLUDE_DIRS}
)

# Tool to embed metadata into JP2 file
add_executable(opj_jpip_addxml opj_jpip_addxml.c)
# Install exe
install(TARGETS opj_jpip_addxml
  EXPORT OpenJPEGTargets
  DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
  )

if(BUILD_JPIP_SERVER)
  set(OPJ_SERVER_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/opj_server.c
  )

  # Build executable
  add_executable(opj_server ${OPJ_SERVER_SRCS})
  target_link_libraries(opj_server ${FCGI_LIBRARIES} openjpip_server)
  set_property(
    TARGET opj_server
    APPEND PROPERTY
    COMPILE_DEFINITIONS SERVER QUIT_SIGNAL="quitJPIP"
    )

  # On unix you need to link to the math library:
  if(UNIX)
    target_link_libraries(opj_server m)
  endif()

  # Install exe
  install(TARGETS opj_server
    EXPORT OpenJPEGTargets
    DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
    )
endif()

set(EXES
  opj_dec_server
  opj_jpip_transcode
  opj_jpip_test
  )
foreach(exe ${EXES})
add_executable(${exe} ${exe}.c)
  target_link_libraries(${exe} openjpip)
  install(TARGETS ${exe}
    EXPORT OpenJPEGTargets
    DESTINATION ${OPENJPEG_INSTALL_BIN_DIR} COMPONENT Applications
    )
endforeach()

# Build the two java clients:
find_package(Java 1.5 COMPONENTS Development) # javac, jar

# User can override this:
if(NOT DEFINED JAVA_SOURCE_VERSION)
  set(JAVA_SOURCE_VERSION 1.5)
endif()
if(NOT DEFINED JAVA_TARGET_VERSION)
  set(JAVA_TARGET_VERSION 1.5)
endif()

# Only build the java viewer if dev is found:
if(Java_Development_FOUND AND Java_JAVAC_EXECUTABLE)
  set(jflags $ENV{JFLAGS})
  # search for package org.apache.xerces.parsers
  find_file(APACHE_XERCES_JAR
    NAMES xerces-j2.jar xercesImpl.jar
    PATHS /usr/share/java/
    NO_DEFAULT_PATH
    )
  mark_as_advanced(APACHE_XERCES_JAR)

  # Decide to build the simple viewer or the xerces one:
  if(EXISTS ${APACHE_XERCES_JAR})
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer_xerces/dist/manifest.txt.in
      ${CMAKE_CURRENT_BINARY_DIR}/opj_viewer_xerces/dist/manifest.txt
      @ONLY
      )
    # build dep list:
    file(GLOB java2_srcs "opj_viewer_xerces/src/*.java")

    # Need some common files:
    list(APPEND java2_srcs
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/ImageManager.java
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/ImgdecClient.java
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/JPIPHttpClient.java
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/MML.java
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/PnmImage.java
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/RegimViewer.java
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/src/ResizeListener.java
      )

    # make sure target javac dir exists:
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classes2)
    # Build java
    add_custom_command(
      OUTPUT ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      COMMAND ${Java_JAVAC_EXECUTABLE} ${jflags}
      -source ${JAVA_SOURCE_VERSION} -target ${JAVA_TARGET_VERSION}
      -classpath ${APACHE_XERCES_JAR}
      ${java2_srcs} -d ${CMAKE_CURRENT_BINARY_DIR}/classes2
      COMMAND ${Java_JAR_EXECUTABLE} cfm ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      ${CMAKE_CURRENT_BINARY_DIR}/opj_viewer_xerces/dist/manifest.txt
      -C ${CMAKE_CURRENT_BINARY_DIR}/classes2 .
      DEPENDS ${java2_srcs}
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer_xerces/dist/manifest.txt.in
      COMMENT "javac *.java; jar cvf -> opj_viewer_xerces.jar"
      )

    # name the target
    add_custom_target(OPJViewerXercesJar ALL
      DEPENDS ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      COMMENT "building opj_jpip_viewer.jar (xerces)"
      )

    install(FILES ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      DESTINATION ${OPENJPEG_INSTALL_SHARE_DIR} COMPONENT JavaModule
      )
  else()
    # opj_viewer (simple, no xerces)
    # build dep list:
    file(GLOB java1_srcs "opj_viewer/src/*.java")

    # make sure target javac dir exists:
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/classes1)
    # Build java
    add_custom_command(
      OUTPUT ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      COMMAND ${Java_JAVAC_EXECUTABLE} ${jflags}
      -source ${JAVA_SOURCE_VERSION} -target ${JAVA_TARGET_VERSION}
      ${java1_srcs} -d ${CMAKE_CURRENT_BINARY_DIR}/classes1
      COMMAND ${Java_JAR_EXECUTABLE} cfm ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/dist/manifest.txt -C
      ${CMAKE_CURRENT_BINARY_DIR}/classes1 .
      DEPENDS ${java1_srcs}
      ${CMAKE_CURRENT_SOURCE_DIR}/opj_viewer/dist/manifest.txt
      COMMENT "javac *.java; jar cvf -> opj_jpip_viewer.jar"
      )

    # name the target
    add_custom_target(OPJViewerJar ALL
      DEPENDS ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      COMMENT "building opj_jpip_viewer.jar (no xerces found)"
      )

    install(FILES ${LIBRARY_OUTPUT_PATH}/opj_jpip_viewer.jar
      DESTINATION ${OPENJPEG_INSTALL_SHARE_DIR} COMPONENT JavaModule
      )
  endif()
else()
  message(WARNING "No java compiler found. Wont be able to build java viewer")
endif()
