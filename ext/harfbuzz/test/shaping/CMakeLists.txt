if (HB_BUILD_UTILS)
  file (READ "${CMAKE_CURRENT_SOURCE_DIR}/data/in-house/Makefile.sources" INHOUSE)
  extract_make_variable (TESTS ${INHOUSE})
  foreach (test IN ITEMS ${TESTS})
    add_test (NAME ${test}
      COMMAND "${PYTHON_EXECUTABLE}" run-tests.py $<TARGET_FILE:hb-shape> "data/in-house/${test}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endforeach ()

  file (READ "${CMAKE_CURRENT_SOURCE_DIR}/data/text-rendering-tests/Makefile.sources" TEXTRENDERING)
  extract_make_variable (TESTS ${TEXTRENDERING})
  foreach (test IN ITEMS ${TESTS})
    add_test (NAME ${test}
      COMMAND "${PYTHON_EXECUTABLE}" run-tests.py $<TARGET_FILE:hb-shape> "data/text-rendering-tests/${test}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endforeach ()
endif ()
