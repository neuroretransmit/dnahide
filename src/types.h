#pragma once

#include <cstdint>
#include <vector>

using namespace std;

/// 8-bit unsigned integer
typedef uint8_t u8;
/// 16-bit unsgined integer
typedef uint16_t u16;
/// 32-bit unsigned integer
typedef uint32_t u32;
/// 64-bit unsigned integer
typedef uint64_t u64;

/// Block size constant container
namespace WordSize
{
    typedef u32 BLOCK_128;
    typedef u64 BLOCK_256;
}; // namespace WordSize

/// Abstract block cipher interface
template<class T> class CipherInterface
{
  public:
    virtual void encrypt(vector<u8>& data, const vector<u8>& key) = 0;
    virtual void decrypt(vector<u8>& data, const vector<u8>& key) = 0;

    virtual size_t block_size() { return sizeof(T); }
};

template<class T> static inline size_t block_byte_size() { return sizeof(T) * 4; }
