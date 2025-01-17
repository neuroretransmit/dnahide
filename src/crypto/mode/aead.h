#pragma once

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "../rc6.h"
#include "aead/polyval.h"
#include "ctr.h"
#include "ecb.h"

#include "../../obfuscate.h"

using random_bytes_engine = independent_bits_engine<default_random_engine, CHAR_BIT, u8>;

/// Authenticated Encryption with Additional Data
template<class T> class AEAD
{
  public:
    /**
     * Constructor for AEAD.
     * @param key_generating_key: key generating key to derive from
     */
    AEAD(const vector<u8>& key_generating_key) : KEY_GENERATING_KEY(key_generating_key)
    {
        if ((KEY_GENERATING_KEY.size() != (128 / 8) && KEY_GENERATING_KEY.size() != (256 / 8))) {
            string error = "ERROR: Key generating key for AEAD must be 16 or 32 bytes. Got "_hidden;
            cerr << error << KEY_GENERATING_KEY.size() << ".\n";
            exit(1);
        }
    }

    /**
     * Encrypt message with prepended nonce/appended tag
     * @param plaintext: plaintext to encrypt
     * @param aad: additional authenticated data
     */
    void seal(vector<u8>& plaintext, vector<u8>& aad, bool parallel = true)
    {
        // Generate random nonce -- completely disallow misuse
        vector<u8> nonce(NONCE_BYTE_LEN);
        random_bytes_engine rbe;
        generate(begin(nonce), end(nonce), ref(rbe));
        // Encrypt
        seal(plaintext, aad, nonce, parallel);
        // Insert nonce into beginning of  plaintext
        plaintext.insert(plaintext.begin(), nonce.begin(), nonce.end());
    }

    /**
     * Decrypt/authenticate message
     * @param ciphertext: ciphertext to decrypt
     * @param aad: additional authenticated data
     */
    void open(vector<u8>& ciphertext, vector<u8>& aad, bool parallel = true)
    {
        size_t ciphertext_size = ciphertext.size();

        // TODO: Test
        if (ciphertext.size() < NONCE_BYTE_LEN) {
            string error = "ERROR: Ciphertext to be at least the 96 bytes (nonce size), got "_hidden;
            cerr << error << ciphertext_size << "\n";
            exit(1);
        }

        // Retrieve nonce
        const vector<u8> nonce(ciphertext.begin(), ciphertext.begin() + NONCE_BYTE_LEN);
        // Remove nonce from ciphertext
        ciphertext = vector<u8>(ciphertext.begin() + NONCE_BYTE_LEN, ciphertext.end());
        open(ciphertext, aad, nonce, parallel);
    }

#ifndef DEBUG
  private:
#endif

    const size_t NONCE_BYTE_LEN = 96 / 8;
    const size_t BLOCK_BYTE_LEN = block_byte_size<T>();
    /// 64GB data limit for GCM-SIV
    const double MAX_DATA_SIZE = pow(2, 36);
    const vector<u8>& KEY_GENERATING_KEY;

    /**
     * Calculate tag for authentication
     * @param message_encryption_key: vector to store message encryption key in
     * @param message_authentication_key: vector to store message authentication key in
     * @param plaintext: plaintext message
     * @param aad: authenticated additional data
     * @param nonce: nonce
     */
    vector<u8> get_tag(const vector<u8>& message_encryption_key, const vector<u8>& message_authentication_key,
                       const vector<u8>& plaintext, const vector<u8>& aad, const vector<u8>& nonce)
    {
        // Create length block
        vector<u8> length_block(BLOCK_BYTE_LEN);
        u64 aad_bit_len = aad.size() * 8;
        u64 plaintext_bit_len = plaintext.size() * 8;
        in_place_update(length_block, !is_big_endian() ? aad_bit_len : swap_endian(aad_bit_len), 0);
        in_place_update(length_block, !is_big_endian() ? plaintext_bit_len : swap_endian(plaintext_bit_len),
                        8);

        // Digest
        Polyval<T> authenticator = Polyval<T>(message_authentication_key);
        authenticator.update(aad);
        authenticator.update(plaintext);
        authenticator.update(length_block);
        vector<u8> digest = authenticator.digest();

        // XOR first 96 bytes of digest with nonce
        for (size_t i = 0; i < nonce.size(); i++)
            digest[i] ^= nonce[i];

        digest[digest.size() - 1] &= ~0x80;

        // Encrypt digest
        RC6<T> cipher{};
        // TODO: I remember a fancy way to instanciate a template parameter in the
        //       inheriting class
        ECB<RC6<T>> ecb(cipher);
        ecb.encrypt(digest, message_encryption_key);

        return digest;
    }

    /**
     * Encrypt key counter block
     * @param rc6: cipher instance TODO: use abstract cipher interface
     * @param ctr_block: counter block
     * @param key_ctr: key counter
     * @param nonce: nonce
     */
    void encrypt_key_ctr_block(RC6<T>& rc6, vector<u8>& ctr_block, u32 key_ctr, const vector<u8>& nonce)
    {
        u8* byte_arr = (u8*) &key_ctr;

        if (is_big_endian()) {
            T* word_arr = (T*) &key_ctr;
            for (size_t i = 0; i < sizeof(*word_arr) / sizeof(T); i++)
                word_arr[i] = swap_endian(word_arr[i]);
        }

        ctr_block.insert(ctr_block.end(), byte_arr, byte_arr + sizeof(T));

        // Append nonce
        ctr_block.insert(ctr_block.end(), nonce.begin(), nonce.end());
        rc6.encrypt(ctr_block, KEY_GENERATING_KEY);
    }

    /**
     * Derive message authentication/encryption keys
     * @param message_authentication_key: reference to storage location of message
     * authentication key
     * @param message_encryption_key: reference to storage location of message
     * authentication key
     * @param nonce: nonce
     */
    void derive_keys(vector<u8>& message_authentication_key, vector<u8>& message_encryption_key,
                     const vector<u8>& nonce)
    {
        RC6<T> rc6 = RC6<T>();
        u32 key_ctr;

        // 128-bit key key generating key
        for (key_ctr = 0; key_ctr < 4; key_ctr++) {
            vector<u8> ctr_block;
            encrypt_key_ctr_block(rc6, ctr_block, key_ctr, nonce);

            for (size_t j = 0; j < 8; j++) {
                if (key_ctr < 2)
                    message_authentication_key.push_back(ctr_block[j]);
                else if (key_ctr >= 2)
                    message_encryption_key.push_back(ctr_block[j]);
            }
        }

        // 256-bit key generating key
        if (KEY_GENERATING_KEY.size() == 32) {
            for (key_ctr = 4; key_ctr < 6; key_ctr++) {
                vector<u8> ctr_block;
                encrypt_key_ctr_block(rc6, ctr_block, key_ctr, nonce);

                for (size_t j = 0; j < 8; j++)
                    message_encryption_key.push_back(ctr_block[j]);
            }
        }
    }

    /**
     * Validate nonce, plaintext and additional authenticated data
     * @param nonce
     * @param plaintext
     * @param aad: additional authenticated data
     */
    void validate(const vector<u8>& nonce, const vector<u8>& plaintext, const vector<u8>& aad)
    {
        // Validate nonce size
        size_t nonce_size = nonce.size();
        if (nonce_size != NONCE_BYTE_LEN) {
            string error = "ERROR: Nonce must be 96-bits, got "_hidden;
            cerr << error << nonce_size << ".\n";
            exit(1);
        }

        // Validate data is < 64GB for plaintext and AAD
        u64 plaintext_size = plaintext.size();
        u64 aad_size = aad.size();
        if (plaintext_size > MAX_DATA_SIZE) {
            string error = "ERROR: Plaintext must be < 64GB, got "_hidden;
            cerr << error << plaintext_size << ".\n";
            exit(1);
        } else if (aad_size > MAX_DATA_SIZE) {
            string error = "ERROR: Authenticated data must be < 64GB, got "_hidden;
            cerr << error << aad_size << ".\n";
            exit(1);
        }
    }

    /**
     * Remove padding and append tag
     * @param ciphertext
     * @param pad_len: number of padded bytes
     * @param tag
     */
    void trim_padding_append_tag(vector<u8>& ciphertext, size_t pad_len, const vector<u8>& tag)
    {
        // Snip padding length
        ciphertext.erase(ciphertext.end() - pad_len, ciphertext.end());
        // Append tag
        ciphertext.insert(ciphertext.end(), tag.begin(), tag.end());
    }

    /**
     * Encrypt message and append tag
     * @param plaintext: plaintext to encrypt
     * @param aad: additional authenticated data
     * @param nonce: user-provided nonce
     */
    void seal(vector<u8>& plaintext, vector<u8>& aad, const vector<u8>& nonce, bool parallel = true)
    {
        validate(nonce, plaintext, aad);

        // Derive keys
        vector<u8> authentication_key;
        vector<u8> encryption_key;
        derive_keys(authentication_key, encryption_key, nonce);

        // Pad plaintext/AAD
        size_t pad_len = pad_to_block_size(plaintext, BLOCK_BYTE_LEN);
        pad_to_block_size(aad, BLOCK_BYTE_LEN);

        // Calculate tag
        vector<u8> tag = get_tag(encryption_key, authentication_key, plaintext, aad, nonce);

        // Encrypt
        RC6<T> cipher{};
        ECB<RC6<T>> ecb(cipher);
        CTR<ECB<RC6<T>>> ctr(ecb, BLOCK_BYTE_LEN);
        parallel ? ctr.crypt_parallel(plaintext, encryption_key, tag)
                 : ctr.crypt(plaintext, encryption_key, tag);

        trim_padding_append_tag(plaintext, pad_len, tag);
    }

    /**
     * Authenticate decrypted ciphertext
     * @param ciphertext
     * @param encryption_key
     * @param authentication_key
     * @param aad: additional authenticated data
     * @param nonce
     * @param tag
     * @param pad_len: number of bytes padded to blocksize
     */
    void authenticate(vector<u8>& ciphertext, const vector<u8>& encryption_key,
                      const vector<u8>& authentication_key, const vector<u8>& aad, const vector<u8>& nonce,
                      const vector<u8>& tag, size_t pad_len)
    {
        // Replace padded portion of decrypted text with zeroes for tag calculation
        for (size_t i = ciphertext.size() - pad_len; i < ciphertext.size(); i++)
            ciphertext[i] = 0;

        // Calculate tag
        vector<u8> actual = get_tag(encryption_key, authentication_key, ciphertext, aad, nonce);

        // Snip padding to original data size
        ciphertext = vector<u8>(ciphertext.begin(), ciphertext.end() - pad_len);

        // Authenticate TODO: Create custom authentication exception
        if (!equal(actual.begin(), actual.end(), tag.begin())) {
            string msg = "AEAD authentication failed"_hidden;
            throw runtime_error(msg);
        }
    }

    /**
     * Decrypt and authenticate message
     * @param ciphertext: ciphertext to decrypt and authenticate
     * @param aad: additional authenticated data
     * @param nonce: nonce
     */
    void open(vector<u8>& ciphertext, vector<u8>& aad, const vector<u8>& nonce, bool parallel)
    {
        // Extract tag/ciphertext
        vector<u8> tag(ciphertext.end() - BLOCK_BYTE_LEN, ciphertext.end());
        ciphertext = vector<u8>(ciphertext.begin(), ciphertext.end() - BLOCK_BYTE_LEN);

        // Pad plaintext/AAD
        size_t pad_len = pad_to_block_size(ciphertext, BLOCK_BYTE_LEN);
        pad_to_block_size(aad, BLOCK_BYTE_LEN);

        // Derive keys
        vector<u8> authentication_key;
        vector<u8> encryption_key;
        derive_keys(authentication_key, encryption_key, nonce);

        // Decrypt
        RC6<T> cipher{};
        ECB<RC6<T>> ecb(cipher);
        CTR<ECB<RC6<T>>> ctr(ecb, BLOCK_BYTE_LEN);
        parallel ? ctr.crypt_parallel(ciphertext, encryption_key, tag)
                 : ctr.crypt(ciphertext, encryption_key, tag);

        // Authenticate
        authenticate(ciphertext, encryption_key, authentication_key, aad, nonce, tag, pad_len);
    }

    /**
     * Load N-bit value (n) into output vector for plaintext/aad sizes
     * @param bytes: buffer to store representation of n
     * @param n: N-bit value to load into bytes
     * @param offset: offset in bytes vector to insert at
     */
    template<class U> void in_place_update(vector<u8>& bytes, U n, u32 offset = 0)
    {
        for (size_t i = 0, shift = 0; i < sizeof(U); i++, shift += 8)
            bytes[offset + i] = (u8)(n >> shift);
    }
};
