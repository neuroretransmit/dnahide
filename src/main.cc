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
#include "crypto/kdf/fastpbkdf2.h"
#include "crypto/mode/aead.h"
#include "crypto/mode/ctr.h"
#include "dna64.h"
#include "obfuscate.h"

/** TODO: Validate GenBank DNA sequence file format unstegging */

using namespace std;
namespace po = boost::program_options;

enum { SUCCESS, ERROR_IN_COMMAND_LINE, ERROR_UNHANDLED_EXCEPTION, INVALID_GENBANK_FILE };

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

static void encrypt(vector<u8>& data, const string& password, const string& aad = "")
{
    string msg = "[*] Encrypting data..."_hidden;
    cerr << msg << endl;

    if (aad != "") {
        vector<u8> aad_bytes(aad.begin(), aad.end());
        vector<u8> kgk(32);
        fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                               15000, kgk.data(), kgk.size());
        AEAD<WordSize::BLOCK_128> aead(kgk);
        aead.seal(data, aad_bytes, true);
    } else {
        RC6<WordSize::BLOCK_128> cipher{};
        ECB<RC6<WordSize::BLOCK_128>> ecb(cipher);
        CTR<ECB<RC6<WordSize::BLOCK_128>>> ctr(ecb, block_byte_size<WordSize::BLOCK_128>());
        vector<u8> counter(16);
        vector<u8> kgk(32);
        fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                               15000, kgk.data(), kgk.size());
        size_t padding = pad_to_block_size(data, block_byte_size<WordSize::BLOCK_128>());
        ctr.crypt_parallel(data, kgk, counter);
        // Snip padding length
        data.erase(data.end() - padding, data.end());
        // Append tag
        data.insert(data.end(), counter.begin(), counter.end());
    }
}

static void decrypt(vector<u8>& data, const string& password, const string& aad = "")
{
    string msg = "[*] Decrypting data..."_hidden;
    cerr << msg << endl;

    if (aad != "") {
        vector<u8> aad_bytes(aad.begin(), aad.end());
        vector<u8> kgk(32);
        fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                               15000, kgk.data(), kgk.size());
        // Create AEAD using RC6
        AEAD<WordSize::BLOCK_128> aead(kgk);
        aead.open(data, aad_bytes, true);
    } else {
        vector<u8> tag(data.end() - block_byte_size<WordSize::BLOCK_128>(), data.end());
        RC6<WordSize::BLOCK_128> cipher{};
        ECB<RC6<WordSize::BLOCK_128>> ecb(cipher);
        CTR<ECB<RC6<WordSize::BLOCK_128>>> ctr(ecb, block_byte_size<WordSize::BLOCK_128>());
        vector<u8> counter(16);
        vector<u8> kgk(32);
        fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                               15000, kgk.data(), kgk.size());
        pad_to_block_size(data, block_byte_size<WordSize::BLOCK_128>());
        ctr.crypt_parallel(data, kgk, counter);
    }
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
                      const string& output_file, bool disable_compression)
{
    string data;
    stringstream ss;
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
            ss << line << endl;
        cerr << endl << footer;
        cerr << endl;
    }

    // If read from stdin
    if (data == "")
        data = ss.str();

    vector<u8> input_data(data.begin(), data.end());

    if (!disable_compression) {
        string compressing = "[*] Compressing..."_hidden;
        cerr << compressing << endl;
        lzma::compress(data, compressed);
    }

    if (password != "") {
        if (aad != "")
            encrypt(encrypted = disable_compression ? input_data : compressed, password, aad);
        else
            encrypt(encrypted = disable_compression ? input_data : compressed, password);
    }

    string encoding = "[*] Encoding DNA..."_hidden;
    cerr << encoding << endl;
    dna = dna64::encode(password != "" ? encrypted : (disable_compression ? input_data : compressed));
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
                        exit(INVALID_GENBANK_FILE);
                    }
                }
            }
        }
    }

    return dna_ss.str();
}

static void unsteg_data(const string& password, const string& aad, const string& input_file,
                        const string& output_file, bool disable_compression)
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

    if (password != "") {
        if (aad != "")
            decrypt(decrypted, password, aad);
        else
            decrypt(decrypted, password);
    }

    size_t decompressed_size = 0;
    vector<u8> decompressed;
    if (!disable_compression) {
        string decompressing = "[*] Decompressing data..."_hidden;
        cerr << decompressing << endl;
        decompressed = vector<u8>(decrypted.size() * 4);
        decompressed_size = lzma::decompress(decrypted, decompressed);
    }

    if (output_file != "") {
        ofstream ofs(output_file, ios_base::out | ios_base::binary);
        if (!disable_compression)
            ofs.write(reinterpret_cast<const char*>(decompressed.data()), decompressed_size);
        else
            ofs.write(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
        ofs.close();
    } else {
        string header = "<<< BEGIN RECOVERED MESSAGE >>>"_hidden;
        string footer = "<<< END RECOVERED MESSAGE >>>"_hidden;
        cerr << endl << header << endl << endl;
        if (!disable_compression)
            cout.write(reinterpret_cast<const char*>(decompressed.data()), decompressed_size);
        else
            cout.write(reinterpret_cast<const char*>(decrypted.data()), decrypted.size());
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
    bool disable_compression = false;

    try {
        string options = "dnahide options"_hidden;
        string help_switches = "help,h"_hidden, help_message = "print usage"_hidden;
        string unsteg_switches = "unsteg,u"_hidden, unsteg_message = "unsteg message"_hidden;
        string input_switches = "input,i"_hidden, input_message = "input file"_hidden;
        string output_switches = "output,o"_hidden, output_message = "output file"_hidden;
        string pass_switches = "password,p"_hidden, pass_message = "encryption password"_hidden;
        string aad_switches = "aad,a"_hidden, aad_message = "additional authenticated data"_hidden;
        string disable_compression_switches = "disable-compression,dc"_hidden,
               disable_compression_message = "disable compression"_hidden;

        po::options_description desc(options);
        // clang-format off
        desc.add_options()(help_switches.c_str(), help_message.c_str())(
            unsteg_switches.c_str(), po::bool_switch(&unsteg),
            unsteg_message.c_str())(input_switches.c_str(), po::value(&input_file), input_message.c_str())(
            output_switches.c_str(), po::value(&output_file), output_message.c_str())(
            pass_switches.c_str(), po::value(&password), pass_message.c_str())(
            aad_switches.c_str(), po::value(&aad), aad_message.c_str())(
            disable_compression_switches.c_str(), po::bool_switch(&disable_compression), disable_compression_message.c_str());
        // clang-format on

        po::variables_map vm;

        try {
            po::store(po::parse_command_line(argc, argv, desc), vm);

            if (vm.count("help") || vm.count("h") || argc == 1) {
                help(desc);
                return SUCCESS;
            }

            po::notify(vm);

            if (unsteg)
                unsteg_data(password, aad, input_file, output_file, disable_compression);
            else
                steg_data(password, aad, input_file, output_file, disable_compression);
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
