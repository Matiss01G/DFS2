add_test([=[TCP_PeerTest.BasicFrameExchange]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCP_PeerTest.BasicFrameExchange]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.BasicFrameExchange]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.LargeFrameTransmission]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCP_PeerTest.LargeFrameTransmission]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.LargeFrameTransmission]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.BidirectionalCommunication]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCP_PeerTest.BidirectionalCommunication]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.BidirectionalCommunication]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCP_PeerTest.DisconnectedPeerHandling]=]  /home/runner/workspace/build/tcp_peer_tests [==[--gtest_filter=TCP_PeerTest.DisconnectedPeerHandling]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCP_PeerTest.DisconnectedPeerHandling]=]  PROPERTIES WORKING_DIRECTORY /home/runner/workspace/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  tcp_peer_tests_TESTS TCP_PeerTest.BasicFrameExchange TCP_PeerTest.LargeFrameTransmission TCP_PeerTest.BidirectionalCommunication TCP_PeerTest.DisconnectedPeerHandling)
