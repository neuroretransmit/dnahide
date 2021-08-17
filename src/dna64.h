#pragma once

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "obfuscate.h"
#include "types.h"

using namespace std;

namespace dna64
{
    static const string b64_idx = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "0123456789+/"_hidden;

    const vector<string> codons = {
        "ATT"_hidden, "ATC"_hidden, "ATA"_hidden, "CTT"_hidden, "CTC"_hidden, "CTA"_hidden, "CTG"_hidden,
        "TTA"_hidden, "TTG"_hidden, "GTT"_hidden, "GTC"_hidden, "GTA"_hidden, "GTG"_hidden, "TTT"_hidden,
        "TTC"_hidden, "ATG"_hidden, "TGT"_hidden, "TGC"_hidden, "GCT"_hidden, "GCC"_hidden, "GCA"_hidden,
        "GCG"_hidden, "GGT"_hidden, "GGC"_hidden, "GGA"_hidden, "GGG"_hidden, "CCT"_hidden, "CCC"_hidden,
        "CCA"_hidden, "CCG"_hidden, "ACT"_hidden, "ACC"_hidden, "ACA"_hidden, "ACG"_hidden, "TCT"_hidden,
        "TCC"_hidden, "TCA"_hidden, "TCG"_hidden, "AGT"_hidden, "AGC"_hidden, "TAT"_hidden, "TAC"_hidden,
        "TGG"_hidden, "CAA"_hidden, "CAG"_hidden, "AAT"_hidden, "AAC"_hidden, "CAT"_hidden, "CAC"_hidden,
        "GAA"_hidden, "GAG"_hidden, "GAT"_hidden, "GAC"_hidden, "AAA"_hidden, "AAG"_hidden, "CGT"_hidden,
        "CGC"_hidden, "CGA"_hidden, "CGG"_hidden, "AGA"_hidden, "AGG"_hidden, "TAA"_hidden, "TAG"_hidden,
        "TGA"_hidden};

    string encode(const vector<u8>& bytes)
    {
        size_t bytes_len = bytes.size();
        string ret;
        ret.reserve((4 * (bytes_len / 3)) * 3);
        stringstream encoded(ret);

        int i = 0, j = 0;
        u8 chunk3b[3], chunk4b[4];
        int bytes_idx = 0;
        while (bytes_len--) {
            chunk3b[i++] = bytes[bytes_idx++];

            if (i == 3) {
                chunk4b[0] = (chunk3b[0] & 0xfc) >> 2;
                chunk4b[1] = ((chunk3b[0] & 0x03) << 4) + ((chunk3b[1] & 0xf0) >> 4);
                chunk4b[2] = ((chunk3b[1] & 0x0f) << 2) + ((chunk3b[2] & 0xc0) >> 6);
                chunk4b[3] = chunk3b[2] & 0x3f;
                for (i = 0; i < 4; i++) {
                    encoded << codons[chunk4b[i]];
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
                encoded << codons[chunk4b[j]];
            }
        }

        return encoded.str();
    }

    string to_b64(const string& str, int len)
    {
        int num_substr = str.length() / len;
        vector<string> split;
        for (auto i = 0; i <= num_substr; i++)
            split.push_back(str.substr(i * len, len));
        set<string> to_replace(split.begin(), split.end());
        for (const string& codon : to_replace) {
            string b64_replacement =
                string(1, b64_idx[distance(codons.begin(), find(codons.begin(), codons.end(), codon))]);
            replace(split.begin(), split.end(), codon, b64_replacement);
        }
        ostringstream os;
        copy(split.begin(), split.end(), ostream_iterator<string>(os));
        os.seekp(0, ios::end);
        // Pad
        while ((4 - os.tellp() % 4) % 4)
            os << '=';
        return os.str();
    }

    inline bool is_b64(char c) { return (isalnum(c) || (c == '+') || (c == '/')); }

    string b64_decode(string const& encoded)
    {
        int in_len = encoded.size();
        int i = 0;
        int j = 0;
        int in_ = 0;
        u8 chunk4b[4], chunk3b[3];
        stringstream ss;

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
                    ss << chunk3b[i];
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
                ss << chunk3b[j];
        }

        return ss.str();
    }

    string decode(const string& data) { return b64_decode(to_b64(data, 3)); }
}; // namespace dna64
