add_test([=[TCPPeerTest.SendReceiveMessageFrame]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCPPeerTest.SendReceiveMessageFrame]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.SendReceiveMessageFrame]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  tcp_peer_tests_TESTS TCPPeerTest.SendReceiveMessageFrame)
