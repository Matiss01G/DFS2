#include <gtest/gtest.h>
#include <vector>
#include "crypto/crypto_stream.hpp"

using namespace dfs::crypto;

class CryptoStreamTest : public ::testing::Test {
protected:
    CryptoStream crypto;
    std::vector<uint8_t> key;
    std::vector<uint8_t> iv;

    void SetUp() override {
        // Initialize with test key and IV
        key.resize(CryptoStream::KEY_SIZE, 0x42);
        iv.resize(CryptoStream::IV_SIZE, 0x24);
        crypto.initialize(key, iv);
    }
};

TEST_F(CryptoStreamTest, EncryptDecryptRoundtrip) {
    std::vector<uint8_t> original = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    auto encrypted = crypto.encryptStream(original);
    ASSERT_NE(encrypted, original);
    
    auto decrypted = crypto.decryptStream(encrypted);
    ASSERT_EQ(decrypted, original);
}

TEST_F(CryptoStreamTest, InvalidKeySize) {
    std::vector<uint8_t> invalid_key(16, 0x42);  // Wrong size
    std::vector<uint8_t> valid_iv(CryptoStream::IV_SIZE, 0x24);
    
    EXPECT_THROW(crypto.initialize(invalid_key, valid_iv), InitializationError);
}

TEST_F(CryptoStreamTest, InvalidIVSize) {
    std::vector<uint8_t> valid_key(CryptoStream::KEY_SIZE, 0x42);
    std::vector<uint8_t> invalid_iv(8, 0x24);  // Wrong size
    
    EXPECT_THROW(crypto.initialize(valid_key, invalid_iv), InitializationError);
}

TEST_F(CryptoStreamTest, LargeDataStream) {
    std::vector<uint8_t> large_data(1024 * 1024);  // 1MB of data
    for (size_t i = 0; i < large_data.size(); ++i) {
        large_data[i] = i & 0xFF;
    }
    
    auto encrypted = crypto.encryptStream(large_data);
    auto decrypted = crypto.decryptStream(encrypted);
    
    ASSERT_EQ(decrypted, large_data);
}
