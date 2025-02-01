add_test([=[BootstrapTest.GetFile]=]  /home/runner/workspace/build/bootstrap_tests [==[--gtest_filter=BootstrapTest.GetFile]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[BootstrapTest.GetFile]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  bootstrap_tests_TESTS BootstrapTest.GetFile)
