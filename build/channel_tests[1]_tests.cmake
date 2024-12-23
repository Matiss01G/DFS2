add_test([=[ChannelTest.InitialState]=]  /home/runner/StreamCryptoDFS/build/channel_tests [==[--gtest_filter=ChannelTest.InitialState]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ChannelTest.InitialState]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ChannelTest.SingleProduceConsume]=]  /home/runner/StreamCryptoDFS/build/channel_tests [==[--gtest_filter=ChannelTest.SingleProduceConsume]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ChannelTest.SingleProduceConsume]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ChannelTest.MultipleMessages]=]  /home/runner/StreamCryptoDFS/build/channel_tests [==[--gtest_filter=ChannelTest.MultipleMessages]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ChannelTest.MultipleMessages]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ChannelTest.ConsumeEmptyChannel]=]  /home/runner/StreamCryptoDFS/build/channel_tests [==[--gtest_filter=ChannelTest.ConsumeEmptyChannel]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ChannelTest.ConsumeEmptyChannel]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ChannelTest.ConcurrentProducersConsumers]=]  /home/runner/StreamCryptoDFS/build/channel_tests [==[--gtest_filter=ChannelTest.ConcurrentProducersConsumers]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ChannelTest.ConcurrentProducersConsumers]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ChannelTest.AlternatingProduceConsume]=]  /home/runner/StreamCryptoDFS/build/channel_tests [==[--gtest_filter=ChannelTest.AlternatingProduceConsume]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ChannelTest.AlternatingProduceConsume]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  channel_tests_TESTS ChannelTest.InitialState ChannelTest.SingleProduceConsume ChannelTest.MultipleMessages ChannelTest.ConsumeEmptyChannel ChannelTest.ConcurrentProducersConsumers ChannelTest.AlternatingProduceConsume)
