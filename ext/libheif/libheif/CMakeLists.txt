include(CMakePackageConfigHelpers)

configure_file(heif_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/heif_version.h)

set(libheif_headers
    heif.h
    heif_cxx.h
    heif_plugin.h
    ${CMAKE_CURRENT_BINARY_DIR}/heif_version.h)

add_library(heif
    bitstream.cc
    box.cc
    error.cc
    heif.cc
    heif_context.cc
    heif_file.cc
    heif_image.cc
    heif_hevc.cc
    heif_avif.cc
    heif_colorconversion.cc
    heif_plugin_registry.cc
    heif_plugin.cc
    nclx.cc
    bitstream.h
    box.h
    error.h
    heif_api_structs.h
    heif_context.h
    heif_file.h
    heif_image.h
    heif_hevc.h
    heif_avif.h
    heif_colorconversion.h
    heif_plugin_registry.h
    heif_limits.h
    nclx.h
    logging.h
    ${libheif_headers}
)

# Needed to find libheif/heif_version.h while compiling the library
target_include_directories(heif PRIVATE ${libheif_BINARY_DIR} ${libheif_SOURCE_DIR})

# Propagate include/libheif to consume the headers from other projects
target_include_directories(heif
                           PUBLIC
                               $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}>
                               $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                               $<INSTALL_INTERFACE:include>)

set_target_properties(heif
                      PROPERTIES
                          VERSION ${PROJECT_VERSION}
                          SOVERSION ${PROJECT_VERSION_MAJOR})

target_compile_definitions(heif
                           PUBLIC
                               LIBHEIF_EXPORTS
                               HAVE_VISIBILITY)

if(LIBDE265_FOUND)
    target_compile_definitions(heif PRIVATE HAVE_LIBDE265=1)
    target_sources(heif
                   PRIVATE
                       heif_decoder_libde265.cc
                       heif_decoder_libde265.h)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBDE265_CFLAGS}")

    if (NOT "${LIBDE265_LIBRARY_DIRS}" STREQUAL "")
        set(LIBDE265_LINKDIR "-L${LIBDE265_LIBRARY_DIRS}")
    endif()

    include_directories(SYSTEM ${LIBDE265_INCLUDE_DIR})
    target_link_libraries(heif PRIVATE ${LIBDE265_LIBRARIES} ${LIBDE265_LINKDIR})
endif()

if(X265_FOUND)
    target_compile_definitions(heif PRIVATE HAVE_X265=1)
    target_sources(heif PRIVATE
        heif_encoder_x265.cc
        heif_encoder_x265.h
    )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${X265_CFLAGS}")

    if (NOT "${X265_LIBRARY_DIRS}" STREQUAL "")
        set(X265_LINKDIR "-L${X265_LIBRARY_DIRS}")
    endif()

    include_directories(SYSTEM ${X265_INCLUDE_DIR})
    target_link_libraries(heif PRIVATE ${X265_LIBRARIES} ${X265_LINKDIR})
endif()

if(AOM_ENCODER_FOUND OR AOM_DECODER_FOUND)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${AOM_CFLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AOM_CFLAGS}")

  if (NOT "${AOM_LIBRARY_DIRS}" STREQUAL "")
    set(AOM_LINKDIR "-L${AOM_LIBRARY_DIRS}")
  endif()

  include_directories(SYSTEM ${AOM_INCLUDE_DIR})
  target_link_libraries(heif PRIVATE ${AOM_LIBRARIES} ${AOM_LINKDIR})
endif()
if(AOM_ENCODER_FOUND)
  target_compile_definitions(heif PRIVATE HAVE_AOM_ENCODER=1)
  target_sources(heif PRIVATE
    heif_encoder_aom.cc
    heif_encoder_aom.h
  )
endif()
if(AOM_DECODER_FOUND)
  target_compile_definitions(heif PRIVATE HAVE_AOM_DECODER=1)
  target_sources(heif PRIVATE
    heif_decoder_aom.cc
    heif_decoder_aom.h
  )
endif()

if(RAV1E_FOUND)
    target_compile_definitions(heif PRIVATE HAVE_RAV1E=1)
    target_sources(heif PRIVATE
            heif_encoder_rav1e.cc
            heif_encoder_rav1e.h
            )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${RAV1E_CFLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RAV1E_CFLAGS}")

    if (NOT "${RAV1E_LIBRARY_DIRS}" STREQUAL "")
        set(RAV1E_LINKDIR "-L${RAV1E_LIBRARY_DIRS}")
    endif()

    include_directories(SYSTEM ${RAV1E_INCLUDE_DIR})
    target_link_libraries(heif PRIVATE ${RAV1E_LIBRARIES} ${RAV1E_LINKDIR})
endif()

if(DAV1D_FOUND)
    target_compile_definitions(heif PRIVATE HAVE_DAV1D=1)
    target_sources(heif PRIVATE
            heif_decoder_dav1d.cc
            heif_decoder_dav1d.h
            )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${DAV1D_CFLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DAV1D_CFLAGS}")

    if (NOT "${DAV1D_LIBRARY_DIRS}" STREQUAL "")
        set(DAV1D_LINKDIR "-L${DAV1D_LIBRARY_DIRS}")
    endif()

    include_directories(SYSTEM ${DAV1D_INCLUDE_DIR})
    target_link_libraries(heif PRIVATE ${DAV1D_LIBRARIES} ${DAV1D_LINKDIR})
endif ()

write_basic_package_version_file(${PROJECT_NAME}-config-version.cmake COMPATIBILITY ExactVersion)

install(TARGETS heif EXPORT ${PROJECT_NAME}-config
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${libheif_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}-config DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake DESTINATION
    "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
