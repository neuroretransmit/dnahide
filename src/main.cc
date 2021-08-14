#include <fstream>
#include <iostream>
#include <random>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <vector>

#include <zlib.h>

#include <boost/program_options.hpp>

#include "crypto/aead.h"
#include "crypto/fastpbkdf2.h"
#include "dna64.h"

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
    cout << "dnahide - hide messages in DNA" << endl << desc << endl;
}

static string read_file(const string& path)
{
    ifstream input_file(path);
    if (!input_file.is_open()) {
        cerr << "Could not open the file - '" << path << "'" << endl;
        exit(EXIT_FAILURE);
    }
    return string{istreambuf_iterator<char>{input_file}, {}};
}

static void compress_memory(void* data, size_t data_size, vector<u8>& out_data)
{
    vector<uint8_t> buffer;

    const size_t BUFSIZE = 128 * 1024;
    u8 temp_buffer[BUFSIZE];

    z_stream strm;
    strm.zalloc = 0;
    strm.zfree = 0;
    strm.next_in = reinterpret_cast<u8*>(data);
    strm.avail_in = data_size;
    strm.next_out = temp_buffer;
    strm.avail_out = BUFSIZE;

    deflateInit(&strm, Z_BEST_COMPRESSION);

    while (strm.avail_in != 0) {
        int res = deflate(&strm, Z_NO_FLUSH);
        assert(res == Z_OK);
        if (strm.avail_out == 0) {
            buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
            strm.next_out = temp_buffer;
            strm.avail_out = BUFSIZE;
        }
    }

    int deflate_res = Z_OK;
    while (deflate_res == Z_OK) {
        if (strm.avail_out == 0) {
            buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
            strm.next_out = temp_buffer;
            strm.avail_out = BUFSIZE;
        }
        deflate_res = deflate(&strm, Z_FINISH);
    }

    assert(deflate_res == Z_STREAM_END);
    buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE - strm.avail_out);
    deflateEnd(&strm);
    out_data.swap(buffer);
}

int decompress_memory(const void* data, int data_size, vector<u8>& out_data)
{
    z_stream strm;
    strm.total_in = strm.avail_in = data_size;
    strm.next_in = (u8*) data;
    strm.next_out = (u8*) out_data.data();

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.total_out = deflateBound(&strm, data_size);
    strm.avail_out = out_data.size();

    int err = -1;
    int ret = -1;

    err = inflateInit2(
        &strm, (15 + 32)); // 15 window bits, and the +32 tells zlib to to detect if using gzip or zlib
    if (err == Z_OK) {
        err = inflate(&strm, Z_FINISH);
        if (err == Z_STREAM_END) {
            ret = strm.total_out;
        } else {
            inflateEnd(&strm);
            return err;
        }
    } else {
        inflateEnd(&strm);
        return err;
    }

    inflateEnd(&strm);
    return ret;
}

static void encrypt(vector<u8>& data, const string& password, const string& aad)
{
    cerr << "[*] Encrypting data..." << endl;
    vector<u8> aad_bytes(aad.begin(), aad.end());
    vector<u8> kgk(32);
    fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                           15000, kgk.data(), kgk.size());
    AEAD<WordSize::BLOCK_128> aead(kgk);
    aead.seal(data, aad_bytes);
}

static void decrypt(vector<u8>& data, const string& password, const string& aad)
{
    cerr << "[*] Decrypting data..." << endl;
    vector<u8> aad_bytes(aad.begin(), aad.end());
    vector<u8> kgk(32);
    fastpbkdf2_hmac_sha256((u8*) password.data(), password.size(), (u8*) password.data(), password.size(),
                           15000, kgk.data(), kgk.size());
    // Create AEAD using RC6
    AEAD<WordSize::BLOCK_128> aead(kgk);
    aead.open(data, aad_bytes);
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
    ss << setw(12) << left << "LOCUS"
       << "GXP_" << accession << "(PAX6/human) " << setw(4) << dna.length() << " bp " << setw(5) << "DNA"
       << endl
       << setw(12) << left << "ACCESSION"
       << "GXP_" << accession << endl
       << setw(12) << left << "BASE COUNT" << setw(3) << left << count(dna.begin(), dna.end(), 'A') << " a "
       << setw(3) << left << count(dna.begin(), dna.end(), 'C') << " c " << setw(3) << left
       << count(dna.begin(), dna.end(), 'G') << " g " << setw(3) << left << count(dna.begin(), dna.end(), 'T')
       << " t " << endl
       << "ORIGIN" << endl
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
            cerr << "[*] File size: " << file_size(input_file.c_str()) << " bytes" << endl;
            data = read_file(input_file);
        } else {
            cerr << "ERROR: File '" << input_file << "' does not exist.\n";
            exit(ERROR_IN_COMMAND_LINE);
        }
    } else {
        cerr << "<<< BEGIN STEGGED MESSAGE (Press CTRL+D when done) >>>\n\n";
        for (string line; getline(cin, line);)
            data += line + "\n";
        cerr << endl << "<<< END STEGGED MESSAGE >>>\n\n";
        cerr << endl;
    }

    cerr << "[*] Compressing..." << endl;
    compress_memory((void*) data.data(), data.size(), compressed);

    if (password != "")
        encrypt(encrypted = compressed, password, aad);

    cerr << "[*] Encoding DNA..." << endl;
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

// TODO: Capture piped input
static void unsteg_data(const string& password, const string& aad, const string& input_file,
                        const string& output_file)
{
    string data = "";

    if (input_file != "") {
        if (file_exists(input_file)) {
            data = read_file(input_file);
        } else {
            cerr << "ERROR: File '" << input_file << "' does not exist.\n";
            exit(ERROR_IN_COMMAND_LINE);
        }
    } else {
        cout << "<<< BEGIN DNA SEQUENCE MESSAGE (Press CTRL+D when done) >>>\n\n";
        for (string line; getline(cin, line);)
            data += line + "\n";
        cout << endl << "<<< END DNA SEQUENCE MESSAGE >>>\n\n";
    }

    string tmp;
    istringstream iss(data);
    string locus_pattern("LOCUS.*"), accession_pattern("ACCESSION.*"), base_count_pattern("BASE COUNT.*"),
        origin_pattern("ORIGIN.*"), row_number_pattern("\\d+"), dna_pattern("([TAGC]{1,10})"),
        whitespace_pattern("\\s+");
    regex locus_regex(locus_pattern), accession_regex(accession_pattern),
        base_count_regex(base_count_pattern), origin_regex(origin_pattern),
        row_number_regex(row_number_pattern), dna_regex(dna_pattern), whitespace_regex(whitespace_pattern);
    tmp = regex_replace(data, locus_regex, "");
    tmp = regex_replace(tmp, accession_regex, "");
    tmp = regex_replace(tmp, base_count_regex, "");
    tmp = regex_replace(tmp, origin_regex, "");
    tmp = regex_replace(tmp, row_number_regex, "");
    tmp = regex_replace(tmp, dna_regex, "$1");
    tmp = regex_replace(tmp, whitespace_regex, "");
    data = tmp;

    cerr << "[*] Decoding DNA..." << endl;
    data = dna64::decode(data);
    vector<u8> decrypted(data.begin(), data.end());

    if (password != "")
        decrypt(decrypted, password, aad);

    cerr << "[*] Decompressing data..." << endl;
    vector<u8> decompressed(decrypted.size() * 10);
    size_t bytes = decompress_memory(decrypted.data(), decrypted.size(), decompressed);

    if (output_file != "") {
        ofstream ofs(output_file, ios_base::out | ios_base::binary);
        ofs.write(reinterpret_cast<const char*>(decompressed.data()), bytes);
        ofs.close();
    } else {
        cerr << endl << "<<< BEGIN RECOVERED MESSAGE >>>" << endl << endl;
        cout.write(reinterpret_cast<const char*>(decompressed.data()), bytes);
        cerr << endl << "<<< END RECOVERED MESSAGE >>>" << endl;
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
                cerr << "ERROR: "
                     << "must provide authentication data" << endl;
                exit(ERROR_IN_COMMAND_LINE);
            }

            if (unsteg)
                unsteg_data(password, aad, input_file, output_file);
            else
                steg_data(password, aad, input_file, output_file);
        } catch (po::error& e) {
            cerr << "ERROR: " << e.what() << endl << endl;
            cerr << desc << endl;
            return ERROR_IN_COMMAND_LINE;
        } catch (boost::bad_any_cast& e) {
            cerr << "ERROR: " << e.what() << endl << endl;
            cerr << desc << endl;
        }
    } catch (exception& e) {
        cerr << "Unhandled Exception reached the top of main: " << e.what() << ", application will now exit"
             << endl;
        return ERROR_UNHANDLED_EXCEPTION;
    }

    return SUCCESS;
}
