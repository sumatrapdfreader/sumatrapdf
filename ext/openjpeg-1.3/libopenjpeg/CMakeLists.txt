INCLUDE_REGULAR_EXPRESSION("^.*$")
# Defines the source code for the library
SET(OPENJPEG_SRCS
  bio.c
  cio.c
  dwt.c
  event.c
  image.c
  j2k.c
  j2k_lib.c
  jp2.c
  jpt.c
  mct.c
  mqc.c
  openjpeg.c
  pi.c
  raw.c
  t1.c
  t2.c
  tcd.c
  tgt.c
)

# Pass proper definition to preprocessor to generate shared lib
IF(WIN32)
  IF(BUILD_SHARED_LIBS)
    ADD_DEFINITIONS(-DOPJ_EXPORTS)
  ELSE(BUILD_SHARED_LIBS)
    ADD_DEFINITIONS(-DOPJ_STATIC)
  ENDIF(BUILD_SHARED_LIBS)
ENDIF(WIN32)

# Create the library
ADD_LIBRARY(${OPENJPEG_LIBRARY_NAME} ${OPENJPEG_SRCS})
SET_TARGET_PROPERTIES(${OPENJPEG_LIBRARY_NAME} PROPERTIES
	${OPENJPEG_LIBRARY_PROPERTIES})

# Install library
INSTALL(TARGETS ${OPENJPEG_LIBRARY_NAME}
	DESTINATION lib)

# Install includes files
INSTALL(FILES openjpeg.h
	DESTINATION ${INCLUDE_INSTALL_DIR}/openjpeg)
