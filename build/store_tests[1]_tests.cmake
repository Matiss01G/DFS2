add_test([=[StoreTest.BasicStoreAndRetrieve]=]  /home/runner/StreamCryptoDFS/build/store_tests [==[--gtest_filter=StoreTest.BasicStoreAndRetrieve]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[StoreTest.BasicStoreAndRetrieve]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[StoreTest.EmptyInput]=]  /home/runner/StreamCryptoDFS/build/store_tests [==[--gtest_filter=StoreTest.EmptyInput]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[StoreTest.EmptyInput]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[StoreTest.MultipleFiles]=]  /home/runner/StreamCryptoDFS/build/store_tests [==[--gtest_filter=StoreTest.MultipleFiles]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[StoreTest.MultipleFiles]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[StoreTest.ErrorHandling]=]  /home/runner/StreamCryptoDFS/build/store_tests [==[--gtest_filter=StoreTest.ErrorHandling]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[StoreTest.ErrorHandling]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[StoreTest.Clear]=]  /home/runner/StreamCryptoDFS/build/store_tests [==[--gtest_filter=StoreTest.Clear]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[StoreTest.Clear]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  store_tests_TESTS StoreTest.BasicStoreAndRetrieve StoreTest.EmptyInput StoreTest.MultipleFiles StoreTest.ErrorHandling StoreTest.Clear)
