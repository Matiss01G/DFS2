add_test([=[TCP_PeerTest.BasicConnection]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCP_PeerTest.BasicConnection]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.BasicConnection]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.SendData]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCP_PeerTest.SendData]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.SendData]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  tcp_peer_tests_TESTS TCP_PeerTest.BasicConnection TCP_PeerTest.SendData)
