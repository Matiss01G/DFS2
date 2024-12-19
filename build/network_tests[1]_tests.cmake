add_test([=[TCPPeerTest.ConnectionTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCPPeerTest.ConnectionTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.ConnectionTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCPPeerTest.DisconnectionTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCPPeerTest.DisconnectionTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.DisconnectionTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCPPeerTest.SendStreamTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCPPeerTest.SendStreamTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.SendStreamTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCPPeerTest.ReceiveStreamTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCPPeerTest.ReceiveStreamTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.ReceiveStreamTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCPPeerTest.MultipleMessagesTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCPPeerTest.MultipleMessagesTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.MultipleMessagesTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[TCPPeerTest.EdgeCasesTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=TCPPeerTest.EdgeCasesTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[TCPPeerTest.EdgeCasesTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
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
add_test([=[PeerManagerTest.Initialization]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.Initialization]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.Initialization]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.AddValidPeer]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.AddValidPeer]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.AddValidPeer]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.AddNullPeer]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.AddNullPeer]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.AddNullPeer]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.AddDuplicatePeer]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.AddDuplicatePeer]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.AddDuplicatePeer]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.RemoveExistingPeer]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.RemoveExistingPeer]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.RemoveExistingPeer]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.RemoveNonExistentPeer]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.RemoveNonExistentPeer]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.RemoveNonExistentPeer]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.ShutdownTest]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.ShutdownTest]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.ShutdownTest]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.MultiplePeerOperations]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.MultiplePeerOperations]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.MultiplePeerOperations]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.ConcurrentOperations]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.ConcurrentOperations]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.ConcurrentOperations]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.ConcurrentAddRemove]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.ConcurrentAddRemove]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.ConcurrentAddRemove]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.ShutdownErrorHandling]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.ShutdownErrorHandling]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.ShutdownErrorHandling]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.ConcurrentModificationDuringShutdown]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.ConcurrentModificationDuringShutdown]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.ConcurrentModificationDuringShutdown]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.PeerConnectionStatus]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.PeerConnectionStatus]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.PeerConnectionStatus]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.LargePeerCount]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.LargePeerCount]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.LargePeerCount]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[PeerManagerTest.ConcurrentGetPeer]=]  /home/runner/StreamCryptoDFS/build/network_tests [==[--gtest_filter=PeerManagerTest.ConcurrentGetPeer]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[PeerManagerTest.ConcurrentGetPeer]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  network_tests_TESTS TCPPeerTest.ConnectionTest TCPPeerTest.DisconnectionTest TCPPeerTest.SendStreamTest TCPPeerTest.ReceiveStreamTest TCPPeerTest.MultipleMessagesTest TCPPeerTest.EdgeCasesTest ConnectionStateTest.InitialState ConnectionStateTest.ValidTransitions ConnectionStateTest.InvalidTransitions ConnectionStateTest.ErrorStateHandling ConnectionStateTest.TerminalStates ConnectionStateTest.StateToString PeerManagerTest.Initialization PeerManagerTest.AddValidPeer PeerManagerTest.AddNullPeer PeerManagerTest.AddDuplicatePeer PeerManagerTest.RemoveExistingPeer PeerManagerTest.RemoveNonExistentPeer PeerManagerTest.ShutdownTest PeerManagerTest.MultiplePeerOperations PeerManagerTest.ConcurrentOperations PeerManagerTest.ConcurrentAddRemove PeerManagerTest.ShutdownErrorHandling PeerManagerTest.ConcurrentModificationDuringShutdown PeerManagerTest.PeerConnectionStatus PeerManagerTest.LargePeerCount PeerManagerTest.ConcurrentGetPeer)
