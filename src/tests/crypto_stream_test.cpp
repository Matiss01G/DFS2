// #include <gtest/gtest.h>
// #include <sstream>
// #include <vector>
// #include <cstring>
// #include "crypto/crypto_stream.hpp"

// using namespace dfs::crypto;

// class CryptoStreamTest : public ::testing::Test {
// protected:
//     CryptoStream crypto;
//     std::vector<uint8_t> key;
//     std::vector<uint8_t> iv;

//     void SetUp() override {
//         // Initialize with test key and IV
//         key.resize(CryptoStream::KEY_SIZE, 0x42);
//         iv.resize(CryptoStream::IV_SIZE, 0x24);
//         crypto.initialize(key, iv);
//     }

//     // Helper to verify stream contents match
//     static bool streamsEqual(std::istream& s1, std::istream& s2) {
//         // Reset stream positions and states
//         s1.clear();
//         s2.clear();
//         s1.seekg(0);
//         s2.seekg(0);

//         if (!s1.good() || !s2.good()) return false;

//         constexpr size_t BUFSIZE = 8192;  // Larger buffer for efficiency
//         std::vector<char> buf1(BUFSIZE), buf2(BUFSIZE);

//         while (true) {
//             s1.read(buf1.data(), BUFSIZE);
//             s2.read(buf2.data(), BUFSIZE);

//             auto count1 = s1.gcount();
//             auto count2 = s2.gcount();

//             if (count1 != count2) return false;
//             if (count1 == 0) break;

//             if (std::memcmp(buf1.data(), buf2.data(), count1) != 0) return false;

//             if (s1.eof() != s2.eof()) return false;
//         }

//         return s1.eof() && s2.eof();
//     }
// };

// // Test basic encryption and decryption with streams
// TEST_F(CryptoStreamTest, BasicStreamOperation) {
//     const std::string plaintext = "Hello, World! This is a test of stream encryption.";
//     std::stringstream input(plaintext);
//     std::stringstream encrypted;
//     std::stringstream decrypted;

//     // Encrypt
//     crypto.encrypt(input, encrypted);
//     ASSERT_TRUE(encrypted.str() != plaintext);

//     // Decrypt
//     crypto.decrypt(encrypted, decrypted);
//     ASSERT_EQ(decrypted.str(), plaintext);
// }

// // Test error handling for uninitialized crypto
// TEST_F(CryptoStreamTest, UninitializedError) {
//     CryptoStream uninitialized;
//     std::stringstream input("test"), output;
//     EXPECT_THROW(uninitialized.encrypt(input, output), InitializationError);
// }

// // Test handling of empty streams
// TEST_F(CryptoStreamTest, EmptyStream) {
//     std::stringstream empty, output;
//     crypto.encrypt(empty, output);
//     ASSERT_FALSE(output.str().empty()); // Should contain at least padding

//     std::stringstream decrypted;
//     crypto.decrypt(output, decrypted);
//     ASSERT_TRUE(decrypted.str().empty());
// }

// // Test handling of large data streams
// TEST_F(CryptoStreamTest, LargeStream) {
//     std::stringstream input;
//     const size_t TEST_SIZE = 1024 * 1024; // 1MB

//     // Generate test data
//     for (size_t i = 0; i < TEST_SIZE; ++i) {
//         input.put(static_cast<char>(i & 0xFF));
//     }

//     std::stringstream encrypted, decrypted;

//     // Process large stream
//     crypto.encrypt(input, encrypted);
//     crypto.decrypt(encrypted, decrypted);

//     ASSERT_TRUE(streamsEqual(input, decrypted));
// }

// // Test invalid stream states
// TEST_F(CryptoStreamTest, InvalidStreamState) {
//     std::stringstream input("test"), output;
//     input.setstate(std::ios::badbit);
//     EXPECT_THROW(crypto.encrypt(input, output), std::runtime_error);

//     input.clear();
//     output.setstate(std::ios::badbit);
//     EXPECT_THROW(crypto.encrypt(input, output), std::runtime_error);
// }

// // Test block alignment handling
// TEST_F(CryptoStreamTest, BlockAlignment) {
//     // Test various input sizes around block boundaries
//     for (size_t size : {15, 16, 17, 31, 32, 33}) {
//         std::stringstream input;
//         for (size_t i = 0; i < size; ++i) {
//             input.put('A' + (i % 26));
//         }

//         std::stringstream encrypted, decrypted;
//         crypto.encrypt(input, encrypted);
//         crypto.decrypt(encrypted, decrypted);

//         input.seekg(0);
//         ASSERT_TRUE(streamsEqual(input, decrypted))
//             << "Failed for size: " << size;
//     }
// }

// // Test IV generation functionality
// TEST_F(CryptoStreamTest, IVGeneration) {
//     CryptoStream crypto_gen;

//     // Test IV size
//     auto iv1 = crypto_gen.generate_IV();
//     ASSERT_EQ(iv1.size(), CryptoStream::IV_SIZE);

//     // Test that multiple calls generate different IVs
//     auto iv2 = crypto_gen.generate_IV();
//     ASSERT_NE(std::memcmp(iv1.data(), iv2.data(), CryptoStream::IV_SIZE), 0)
//         << "Generated IVs should be different";

//     // Test that generated IV can be used for initialization
//     std::vector<uint8_t> test_key(CryptoStream::KEY_SIZE, 0x42);
//     std::vector<uint8_t> test_iv(iv1.begin(), iv1.end());

//     ASSERT_NO_THROW({
//         crypto_gen.initialize(test_key, test_iv);
//     }) << "Generated IV should be valid for initialization";

//     // Test encryption with generated IV
//     std::string test_data = "Test encryption with generated IV";
//     std::stringstream input(test_data);
//     std::stringstream encrypted;
//     std::stringstream decrypted;

//     ASSERT_NO_THROW({
//         crypto_gen.encrypt(input, encrypted);
//         crypto_gen.decrypt(encrypted, decrypted);
//     }) << "Encryption/decryption with generated IV should work";

//     ASSERT_EQ(decrypted.str(), test_data)
//         << "Data encrypted with generated IV should decrypt correctly";
// }