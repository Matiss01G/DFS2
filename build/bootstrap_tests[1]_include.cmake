if(EXISTS "/home/runner/workspace/build/bootstrap_tests[1]_tests.cmake")
  include("/home/runner/workspace/build/bootstrap_tests[1]_tests.cmake")
else()
  add_test(bootstrap_tests_NOT_BUILT bootstrap_tests_NOT_BUILT)
endif()
