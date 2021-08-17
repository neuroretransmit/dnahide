#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <zlib.h>

#include <boost/program_options.hpp>

#include "compression.h"
#include "crypto/aead.h"
#include "crypto/fastpbkdf2.h"
#include "dna64.h"
#include "obfuscate.h"

/** TODO: Validate GenBank DNA sequence file format unstegging */

using namespace std;
namespace po = boost::program_options;

enum { SUCCESS, ERROR_IN_COMMAND_LINE, ERROR_UNHANDLED_EXCEPTION };

static inline bool file_exists(const string& fname)
{
    struct stat buffer;
    return (stat(fname.c_str(), &buffer) == 0);
}

static inline size_t file_size(const char* filename)
{
    ifstream in(filename, ifstream::ate | ifstream::binary);
    return in.tellg();
}

static void help(const po::options_description& desc)
{
    string msg = "dnahide - hide messages in DNA"_hidden;
    cout << msg << endl << desc << endl;
}

static string read_file(const string& path)
{
    ifstream input_file(path);
    string pre = "Could not open the file - '"_hidden;
    string post = "'"_hidden;
    if (!input_file.is_open()) {
        cerr << pre << path << post << endl;
        exit(EXIT_FAILURE);
    }
    return string{istreambuf_iterator<char>{input_file}, {}};
}

static void encrypt(vector<u8>& data, const string& password, const string& aad)
{
    string msg = "[*] Encrypting data..."_hidden;
    cerr << msg << endl;

    vector<u8> aad_bytes(aad.begin(), aad.end());
    vector<u8> kgk(32);
    fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                           15000, kgk.data(), kgk.size());
    AEAD<WordSize::BLOCK_128> aead(kgk);
    aead.seal(data, aad_bytes, false);
}

static void decrypt(vector<u8>& data, const string& password, const string& aad)
{
    string msg = "[*] Decrypting data..."_hidden;
    cerr << msg << endl;
    vector<u8> aad_bytes(aad.begin(), aad.end());
    vector<u8> kgk(32);
    fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                           15000, kgk.data(), kgk.size());
    // Create AEAD using RC6
    AEAD<WordSize::BLOCK_128> aead(kgk);
    aead.open(data, aad_bytes, false);
}

static string create_genbank_flatfile(const string& dna)
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

static void steg_data(const string& password, const string& aad, const string& input_file,
                      const string& output_file)
{
    string data;
    vector<u8> compressed;
    vector<u8> encrypted;
    string dna;

    if (input_file != "") {
        if (file_exists(input_file)) {
            string pre = "[*] File size: "_hidden;
            string post = " bytes"_hidden;
            cerr << pre << file_size(input_file.c_str()) << post << endl;
            data = read_file(input_file);
        } else {
            string pre = "ERROR: File '"_hidden;
            string post = "' does not exist.\n"_hidden;
            cerr << pre << input_file << post;
            exit(ERROR_IN_COMMAND_LINE);
        }
    } else {
        string header = "<<< BEGIN STEGGED MESSAGE (Press CTRL+D when done) >>>\n\n"_hidden;
        string footer = "<<< END STEGGED MESSAGE >>>\n\n"_hidden;
        cerr << header;
        for (string line; getline(cin, line);)
            data += line + "\n";
        cerr << endl << footer;
        cerr << endl;
    }

    string compressing = "[*] Compressing..."_hidden;
    cerr << compressing << endl;
    lzma::compress(data, compressed);

    if (password != "")
        encrypt(encrypted = compressed, password, aad);

    string encoding = "[*] Encoding DNA..."_hidden;
    cerr << encoding << endl;
    dna = dna64::encode(password != "" ? encrypted : compressed);
    dna = create_genbank_flatfile(dna);

    if (output_file != "") {
        ofstream ofs(output_file);
        ofs << dna;
        ofs.close();
    } else {
        cout << endl << dna << endl;
    }
}

static string parse_dna(const string& data)
{
    istringstream iss(data);
    string line;
    bool origin_found = false;
    stringstream dna_ss;

    while (getline(iss, line)) {
        if (line.rfind("ORIGIN"_hidden, 0) == 0) {
            origin_found = true;
            continue;
        } else if (origin_found) {
            for (char& c : line) {
                if (c == 'T' || c == 'A' || c == 'G' || c == 'C') {
                    dna_ss << c;
                }
            }
        }
    }

    return dna_ss.str();
}

static void unsteg_data(const string& password, const string& aad, const string& input_file,
                        const string& output_file)
{
    string data = "";

    if (input_file != "") {
        if (file_exists(input_file)) {
            data = read_file(input_file);
        } else {
            string pre = "ERROR: File '"_hidden;
            string post = "' does not exist.\n"_hidden;
            cerr << pre << input_file << post;
            exit(ERROR_IN_COMMAND_LINE);
        }
    } else {
        string header = "<<< BEGIN DNA SEQUENCE MESSAGE (Press CTRL+D when done) >>>\n\n"_hidden;
        string footer = "<<< END DNA SEQUENCE MESSAGE >>>\n\n"_hidden;
        cout << header;
        for (string line; getline(cin, line);)
            data += line + "\n";
        cout << endl << footer;
    }

    string decoding = "[*] Decoding DNA..."_hidden;
    cerr << decoding << endl;
    string dna = parse_dna(data);
    assert(dna.size() > 0);
    string decoded = dna64::decode(dna);
    vector<u8> decrypted(decoded.begin(), decoded.end());

    if (password != "")
        decrypt(decrypted, password, aad);

    string decompressing = "[*] Decompressing data..."_hidden;
    cerr << decompressing << endl;
    vector<u8> decompressed(decrypted.size() * 4);
    size_t decompressed_size = lzma::decompress(decrypted, decompressed);

    if (output_file != "") {
        ofstream ofs(output_file, ios_base::out | ios_base::binary);
        ofs.write(reinterpret_cast<const char*>(decompressed.data()), decompressed_size);
        ofs.close();
    } else {
        string header = "<<< BEGIN RECOVERED MESSAGE >>>"_hidden;
        string footer = "<<< END RECOVERED MESSAGE >>>"_hidden;
        cerr << endl << header << endl << endl;
        cout.write(reinterpret_cast<const char*>(decompressed.data()), decompressed_size);
        cerr << endl << footer << endl;
    }
}

int main(int argc, char** argv)
{
    bool unsteg = "";
    string password = "";
    string output_file = "";
    string input_file = "";
    string aad = "";

    try {
        po::options_description desc("Options");
        desc.add_options()("help,h", "print help messages")("unsteg,u", po::bool_switch(&unsteg),
                                                            "unsteg message")(
            "input,i", po::value(&input_file), "Input file")("output,o", po::value(&output_file),
                                                             "output file")(
            "password,p", po::value(&password), "encryption password")("aad,a", po::value(&aad),
                                                                       "additional authenticated data");
        po::variables_map vm;

        try {
            // TODO: Validations of invalid switch combinations
            po::store(po::parse_command_line(argc, argv, desc), vm);

            if (vm.count("help") || vm.count("h") || argc == 1) {
                help(desc);
                return SUCCESS;
            }

            po::notify(vm);
            if (password != "" && aad == "") {
                string error = "ERROR: must provide authentication data"_hidden;
                cerr << error << endl;
                exit(ERROR_IN_COMMAND_LINE);
            }

            if (unsteg)
                unsteg_data(password, aad, input_file, output_file);
            else
                steg_data(password, aad, input_file, output_file);
        } catch (po::error& e) {
            string pre = "ERROR: "_hidden;
            cerr << pre << e.what() << endl << endl;
            cerr << desc << endl;
            return ERROR_IN_COMMAND_LINE;
        }
    } catch (exception& e) {
        string pre = "ERROR: unhandled exception reached the top of main: "_hidden;
        string post = ", application will now exit"_hidden;
        cerr << pre << e.what() << post << endl;
        return ERROR_UNHANDLED_EXCEPTION;
    }

    return SUCCESS;
}
