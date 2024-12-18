add_test([=[ConnectionStateTest.InitialState]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=ConnectionStateTest.InitialState]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConnectionStateTest.InitialState]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ConnectionStateTest.ValidTransitions]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=ConnectionStateTest.ValidTransitions]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConnectionStateTest.ValidTransitions]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ConnectionStateTest.InvalidTransitions]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=ConnectionStateTest.InvalidTransitions]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConnectionStateTest.InvalidTransitions]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ConnectionStateTest.ErrorStateHandling]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=ConnectionStateTest.ErrorStateHandling]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConnectionStateTest.ErrorStateHandling]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ConnectionStateTest.TerminalStates]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=ConnectionStateTest.TerminalStates]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConnectionStateTest.TerminalStates]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ConnectionStateTest.StateToString]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=ConnectionStateTest.StateToString]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ConnectionStateTest.StateToString]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.ConnectionLifecycle]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCP_PeerTest.ConnectionLifecycle]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.ConnectionLifecycle]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.ConnectionErrorHandling]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCP_PeerTest.ConnectionErrorHandling]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.ConnectionErrorHandling]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.StreamOperations]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCP_PeerTest.StreamOperations]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.StreamOperations]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.StreamProcessorOperations]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCP_PeerTest.StreamProcessorOperations]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.StreamProcessorOperations]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.ConcurrentOperations]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCP_PeerTest.ConcurrentOperations]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.ConcurrentOperations]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  network_tests_TESTS ConnectionStateTest.InitialState ConnectionStateTest.ValidTransitions ConnectionStateTest.InvalidTransitions ConnectionStateTest.ErrorStateHandling ConnectionStateTest.TerminalStates ConnectionStateTest.StateToString TCP_PeerTest.ConnectionLifecycle TCP_PeerTest.ConnectionErrorHandling TCP_PeerTest.StreamOperations TCP_PeerTest.StreamProcessorOperations TCP_PeerTest.ConcurrentOperations)
