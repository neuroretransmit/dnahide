#pragma once

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include <boost/algorithm/string/join.hpp>

#include "types.h"

using namespace std;
namespace ba = boost::algorithm;

string strip(string in)
{
    in.erase(remove_if(in.begin(), in.end(), [](string::value_type ch) { return !isalpha(ch); }), in.end());
    return in;
}

namespace dna64
{
    static const string b64_idx = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "0123456789+/";

    const vector<string> codons = {
        "ATT", "ATC", "ATA", "CTT", "CTC", "CTA", "CTG", "TTA", "TTG", "GTT", "GTC", "GTA", "GTG",
        "TTT", "TTC", "ATG", "TGT", "TGC", "GCT", "GCC", "GCA", "GCG", "GGT", "GGC", "GGA", "GGG",
        "CCT", "CCC", "CCA", "CCG", "ACT", "ACC", "ACA", "ACG", "TCT", "TCC", "TCA", "TCG", "AGT",
        "AGC", "TAT", "TAC", "TGG", "CAA", "CAG", "AAT", "AAC", "CAT", "CAC", "GAA", "GAG", "GAT",
        "GAC", "AAA", "AAG", "CGT", "CGC", "CGA", "CGG", "AGA", "AGG", "TAA", "TAG", "TGA"};

    string encode(const vector<u8>& bytes)
    {
        string encoded;
        int i = 0, j = 0;
        unsigned char chunk3b[3], chunk4b[4];
        size_t bytes_len = bytes.size();
        int bytes_idx = 0;
        while (bytes_len--) {
            chunk3b[i++] = bytes[bytes_idx++];

            if (i == 3) {
                chunk4b[0] = (chunk3b[0] & 0xfc) >> 2;
                chunk4b[1] = ((chunk3b[0] & 0x03) << 4) + ((chunk3b[1] & 0xf0) >> 4);
                chunk4b[2] = ((chunk3b[1] & 0x0f) << 2) + ((chunk3b[2] & 0xc0) >> 6);
                chunk4b[3] = chunk3b[2] & 0x3f;
                for (i = 0; i < 4; i++) {
                    encoded += codons[chunk4b[i]];
                }
                i = 0;
            }
        }

        if (i) {
            memset(&chunk3b[i], '\0', 3 - i);
            chunk4b[0] = (chunk3b[0] & 0xfc) >> 2;
            chunk4b[1] = ((chunk3b[0] & 0x03) << 4) + ((chunk3b[1] & 0xf0) >> 4);
            chunk4b[2] = ((chunk3b[1] & 0x0f) << 2) + ((chunk3b[2] & 0xc0) >> 6);
            chunk4b[3] = chunk3b[2] & 0x3f;
            for (j = 0; j < i + 1; j++) {
                encoded += codons[chunk4b[j]];
            }
        }

        return encoded;
    }

    string to_b64(const string& str, int len)
    {
        int num_substr = str.length() / len;
        vector<string> split;
        string base64;
        for (auto i = 0; i <= num_substr; i++)
            split.push_back(str.substr(i * len, len));
        for (const string& codon : split) {
            vector<string>::const_iterator itr = find(codons.begin(), codons.end(), codon);
            //             if (itr != codons.end())
            base64 += b64_idx[distance(codons.begin(), itr)];
        }
        //         auto it = std::prev( your_vector.end() );
        //         base64 += b64_idx[distance(codons.begin(), codons.end())];
        while ((4 - base64.length() % 4) % 4)
            base64 += '=';
        return base64;
    }

    inline bool is_b64(char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

    string b64_decode(string const& encoded_string)
    {
        int in_len = encoded_string.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        unsigned char char_array_4[4], char_array_3[3];
        string ret;

        while (in_len-- && (encoded_string[in_] != '=') && is_b64(encoded_string[in_])) {
            char_array_4[i++] = encoded_string[in_];
            in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    char_array_4[i] = b64_idx.find(char_array_4[i]);

                char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
                char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
                char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

                for (i = 0; (i < 3); i++)
                    ret += char_array_3[i];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++)
                char_array_4[j] = 0;

            for (j = 0; j < 4; j++)
                char_array_4[j] = b64_idx.find(char_array_4[j]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (j = 0; (j < i - 1); j++)
                ret += char_array_3[j];
        }

        return ret;
    }

    string decode(const string& data)
    {
        string decoded = to_b64(data, 3);
        return b64_decode(decoded);
    }
}; // namespace dna64
