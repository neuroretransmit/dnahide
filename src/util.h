#pragma once

#include <vector>

using namespace std;

template<typename T> ostream& operator<<(ostream& output, vector<T> const& values)
{
    for (auto const& value : values)
        output << hex << +value << " ";
    return output;
}

/**
 * Check if byte array needs to be padded to block size
 * @param bytes: byte array to check
 * @param block_size: target block size
 */
bool needs_padding(const vector<u8>& bytes, size_t block_size)
{
    return ((bytes.size() < block_size) && block_size - bytes.size()) || (bytes.size() % block_size);
}

size_t pad_to_block_size(vector<u8>& data, size_t block_byte_len)
{
    size_t plaintext_pad_len;
    for (plaintext_pad_len = 0; needs_padding(data, block_byte_len); plaintext_pad_len++)
        data.push_back(0);
    return plaintext_pad_len;
}
