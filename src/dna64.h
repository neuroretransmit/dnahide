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
        u8 chunk3b[3], chunk4b[4];
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
            base64 += b64_idx[distance(codons.begin(), itr)];
        }
        while ((4 - base64.length() % 4) % 4)
            base64 += '=';
        return base64;
    }

    inline bool is_b64(char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

    string b64_decode(string const& encoded)
    {
        int in_len = encoded.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        u8 chunk4b[4], chunk3b[3];
        string ret;

        while (in_len-- && (encoded[in_] != '=') && is_b64(encoded[in_])) {
            chunk4b[i++] = encoded[in_];
            in_++;
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    chunk4b[i] = b64_idx.find(chunk4b[i]);

                chunk3b[0] = (chunk4b[0] << 2) + ((chunk4b[1] & 0x30) >> 4);
                chunk3b[1] = ((chunk4b[1] & 0xf) << 4) + ((chunk4b[2] & 0x3c) >> 2);
                chunk3b[2] = ((chunk4b[2] & 0x3) << 6) + chunk4b[3];

                for (i = 0; (i < 3); i++)
                    ret += chunk3b[i];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++)
                chunk4b[j] = 0;

            for (j = 0; j < 4; j++)
                chunk4b[j] = b64_idx.find(chunk4b[j]);

            chunk3b[0] = (chunk4b[0] << 2) + ((chunk4b[1] & 0x30) >> 4);
            chunk3b[1] = ((chunk4b[1] & 0xf) << 4) + ((chunk4b[2] & 0x3c) >> 2);
            chunk3b[2] = ((chunk4b[2] & 0x3) << 6) + chunk4b[3];

            for (j = 0; (j < i - 1); j++)
                ret += chunk3b[j];
        }

        return ret;
    }

    string decode(const string& data)
    {
        string decoded = to_b64(data, 3);
        return b64_decode(decoded);
    }
}; // namespace dna64
