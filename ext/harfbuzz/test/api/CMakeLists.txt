if (HB_HAVE_GLIB)
  file (READ "${CMAKE_CURRENT_SOURCE_DIR}/Makefile.am" MAKEFILEAM)
  extract_make_variable (TEST_PROGS ${MAKEFILEAM})

  list (APPEND TEST_PROGS
    test-ot-tag
    test-c
    test-cplusplus
  )

  if (HB_HAVE_FREETYPE)
    list (APPEND TEST_PROGS test-ot-math)
  endif ()

  foreach (test_name IN ITEMS ${TEST_PROGS})
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.c)
      add_executable (${test_name} ${test_name}.c)
    elseif (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cc)
      add_executable (${test_name} ${test_name}.cc)
    else ()
      message (FATAL_ERROR "No source file found for test ${test_name}")
    endif ()
    target_link_libraries (${test_name} harfbuzz harfbuzz-subset)
    if (WIN32)
      set_property (TARGET ${test_name} PROPERTY RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
      add_test (NAME ${test_name} COMMAND ${test_name})
    else (WIN32)
      add_test (${test_name} ${test_name})
    endif (WIN32)
  endforeach ()
  set_tests_properties (${TEST_PROGS} PROPERTIES ENVIRONMENT
    "G_TEST_SRCDIR=${CMAKE_CURRENT_SOURCE_DIR};G_TEST_BUILDDIR=${CMAKE_CURRENT_BINARY_DIR}"
  )
endif ()
