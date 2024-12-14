add_test([=[CryptoStreamTest.EncryptDecryptRoundtrip]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=CryptoStreamTest.EncryptDecryptRoundtrip]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[CryptoStreamTest.EncryptDecryptRoundtrip]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[CryptoStreamTest.InvalidKeySize]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=CryptoStreamTest.InvalidKeySize]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[CryptoStreamTest.InvalidKeySize]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[CryptoStreamTest.InvalidIVSize]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=CryptoStreamTest.InvalidIVSize]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[CryptoStreamTest.InvalidIVSize]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[CryptoStreamTest.LargeDataStream]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=CryptoStreamTest.LargeDataStream]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[CryptoStreamTest.LargeDataStream]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ByteOrderTest.NetworkOrderConversion]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=ByteOrderTest.NetworkOrderConversion]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ByteOrderTest.NetworkOrderConversion]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ByteOrderTest.EndianCheck]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=ByteOrderTest.EndianCheck]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ByteOrderTest.EndianCheck]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[ByteOrderTest.DifferentTypes]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=ByteOrderTest.DifferentTypes]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ByteOrderTest.DifferentTypes]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[LoggerTest.BasicLogging]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=LoggerTest.BasicLogging]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[LoggerTest.BasicLogging]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[LoggerTest.ThreadLogging]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=LoggerTest.ThreadLogging]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[LoggerTest.ThreadLogging]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
add_test([=[LoggerTest.SeverityLevels]=]  /home/runner/StreamCryptoDFS/build/crypto_tests [==[--gtest_filter=LoggerTest.SeverityLevels]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[LoggerTest.SeverityLevels]=]  PROPERTIES WORKING_DIRECTORY /home/runner/StreamCryptoDFS/build SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==])
set(  crypto_tests_TESTS CryptoStreamTest.EncryptDecryptRoundtrip CryptoStreamTest.InvalidKeySize CryptoStreamTest.InvalidIVSize CryptoStreamTest.LargeDataStream ByteOrderTest.NetworkOrderConversion ByteOrderTest.EndianCheck ByteOrderTest.DifferentTypes LoggerTest.BasicLogging LoggerTest.ThreadLogging LoggerTest.SeverityLevels)
