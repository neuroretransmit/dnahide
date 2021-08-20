#include "genbank.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "obfuscate.h"

string create_genbank_flatfile(const string& dna)
{
    stringstream ss;
    random_device rd;                                   // obtain a random number from hardware
    mt19937 gen(rd());                                  // seed the generator
    uniform_int_distribution<> distr(0000000, 9999999); // define the range
    int accession = distr(gen);

    // Generate metadata
    // TODO: Lookup GXP codes/PAX6/human and add DESCRIPTION section
    string locus_header = "LOCUS"_hidden;
    string gxp = "GXP_"_hidden;
    string human = "(PAX6/human) "_hidden;
    string bp = " bp "_hidden;
    string dna_header = "DNA"_hidden;
    string accession_header = "ACCESSION"_hidden;
    string base_count = "BASE COUNT"_hidden;
    string t = " t "_hidden, a = " a "_hidden, g = " g "_hidden, c = " c "_hidden;
    string origin_header = "ORIGIN"_hidden;
    ss << setw(12) << left << locus_header << gxp << accession << human << setw(4) << dna.length() << bp
       << setw(5) << dna_header << endl
       << setw(12) << left << accession_header << gxp << accession << endl
       << setw(12) << left << base_count << setw(3) << left << count(dna.begin(), dna.end(), 'A') << a
       << setw(3) << left << count(dna.begin(), dna.end(), 'C') << c << setw(3) << left
       << count(dna.begin(), dna.end(), 'G') << g << setw(3) << left << count(dna.begin(), dna.end(), 'T')
       << t << endl
       << origin_header << endl
       << left;

    int len = 10;
    int num_substr = dna.length() / len;
    vector<string> split;
    for (auto i = 0; i <= num_substr; i++)
        split.push_back(dna.substr(i * len, len));

    size_t j = 0;
    for (size_t i = 0; i < split.size(); i++) {
        ss << setw(9) << right << j + 1 << " ";
        for (j = 0; i < split.size(); j += 10, i += 1) {
            if (j != 0 && !(j % 60))
                ss << endl << setw(9) << right << j + 1 << " ";
            ss << setw(2) << split[i] << " ";
        }
    }

    return ss.str();
}

string parse_dna(const string& data)
{
    istringstream iss(data);
    string line;
    bool origin_found = false;
    stringstream dna_ss;
    const string acceptable = " \n0123456789";

    while (getline(iss, line)) {
        if (line.rfind("ORIGIN"_hidden, 0) == 0) {
            origin_found = true;
            continue;
        } else if (origin_found) {
            for (char& c : line) {
                switch (c) {
                case 'T':
                case 'A':
                case 'G':
                case 'C':
                    dna_ss << c;
                    break;
                default:
                    if (acceptable.find(c) == string::npos) {
                        string error = " ERROR: invalid GenBank file"_hidden;
                        cerr << error << endl;
                    }
                }
            }
        }
    }

    return dna_ss.str();
}
