#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <random>
#include <vector>

#include <zlib.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>

#include "aead.h"
#include "dna64.h"
#include "sha256.h"

/** TODO: Generate GenBank DNA sequence file format for output */

using namespace std;
namespace po = boost::program_options;

enum { SUCCESS, ERROR_IN_COMMAND_LINE, ERROR_UNHANDLED_EXCEPTION };

static inline bool file_exists(const string& fname)
{
    struct stat buffer;
    return (stat(fname.c_str(), &buffer) == 0);
}

size_t filesize(const char* filename)
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

int decompress_memory(const void* data, int data_len, vector<u8>& out_data)
{
    z_stream strm;
    strm.total_in = strm.avail_in = data_len;
    strm.total_out = strm.avail_out = out_data.size();
    strm.next_in = (u8*) data;
    strm.next_out = out_data.data();

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

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

static void encrypt(vector<u8>& data, const string& password)
{
    cerr << "[*] Encrypting data..." << endl;
    // Additional authenticated data is the SHA256 of input file
    vector<u8> aad(32, 0);
    // TODO: Use a key derivation function
    vector<u8> key_generating_key = sha256((char*) password.data());
    // Create AEAD using RC6
    AEAD<WordSize::BLOCK_128> aead(key_generating_key);
    aead.seal(data, aad);
}

static void decrypt(vector<u8>& data, const string& password)
{
    cerr << "[*] Decrypting data..." << endl;
    // Additional authenticated data is the SHA256 of input file
    vector<u8> aad(32, 0);
    // TODO: Use a key derivation function
    vector<u8> key_generating_key = sha256((char*) password.data());
    // Create AEAD using RC6
    AEAD<WordSize::BLOCK_128> aead(key_generating_key);
    aead.open(data, aad);
}


static string create_genbank_flatfile(const string& dna)
{
    stringstream ss;
    random_device rd; // obtain a random number from hardware
    mt19937 gen(rd()); // seed the generator
    uniform_int_distribution<> distr(0000000, 9999999); // define the range
    int accession = distr(gen);

//     size_t i = 1;
    ss << setw(12) << left << "LOCUS" << "GXP_" << accession << "(PAX6/human)    1105 bp    DNA" << endl
        << setw(12) << left << "ACCESSION" << "GXP_" << accession << endl
        << setw(14) << left << "BASE COUNT" 
        << setw(3) << left << count(dna.begin(), dna.end(), 'A') << " a "
        << setw(3) << left << count(dna.begin(), dna.end(), 'C') << " c "
        << setw(3) << left << count(dna.begin(), dna.end(), 'G') << " g "
        << setw(3) << left << count(dna.begin(), dna.end(), 'T') << " t "<< endl
        << "ORIGIN" << endl;
    
    int len = 10;
    int num_substr = dna.length() / len;
    vector<string> split;
    for (auto i = 0; i <= num_substr; i++)
        split.push_back(dna.substr(i * len, len));
    
    size_t j = 0;
    for (size_t i = 0; i < split.size(); i++) {
        ss << setw(9) << right << j + 1 << " ";
        for (j = 0; i < split.size(); j += 10, i += 1) {
            
            if (j != 0 && !(j % 60)) {
                ss << endl << setw(9) << right << j + 1 << " ";
            }
            ss << setw(2) << split[i++] << " ";
        }
    }
    
    return ss.str();
}

static void steg_data(const string& password, const string& input_file, const string& output_file)
{
    string data;
    vector<u8> compressed;
    vector<u8> encrypted;
    string dna;

    if (input_file != "") {
        if (file_exists(input_file)) {
            cerr << "[*] File size: " << filesize(input_file.c_str()) << " bytes" << endl;
            data = read_file(input_file);
        } else {
            cerr << "ERROR: File '" << input_file << "' does not exist.\n";
            exit(ERROR_IN_COMMAND_LINE);
        }
    } else {
        cout << "<<< BEGIN STEGGED MESSAGE (Press CTRL+D when done) >>>\n\n";
        for (string line; getline(cin, line);)
            data += line + "\n";
        cout << endl << "<<< END STEGGED MESSAGE >>>\n\n";
        cout << endl;
    }

    cerr << "[*] Compressing..." << endl;
    compress_memory((void*) data.data(), data.size(), compressed);

    if (password != "")
        encrypt(encrypted = compressed, password);

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

static void unsteg_data(string password, string input_file, string output_file)
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

    cerr << "[*] Decoding DNA..." << endl;
    data = dna64::decode(data);
    vector<u8> decrypted(data.begin(), data.end());
    decrypt(decrypted, password);

    cerr << "[*] Decompressing data..." << endl;
    vector<u8> decompressed(data.size() * 10);
    decompress_memory(decrypted.data(), decrypted.size(), decompressed);

    if (output_file != "") {
        ofstream ofs(output_file);
        ofs << decompressed.data();
        ofs.close();
    } else {
        cout << endl
             << "<<< BEGIN RECOVERED MESSAGE >>>" << endl
             << endl
             << decompressed.data() << endl
             << "<<< END RECOVERED MESSAGE >>>" << endl;
    }
}

int main(int argc, char** argv)
{
    bool unsteg = "";
    string password = "";
    string output_file = "";
    string input_file = "";

    try {
        po::options_description desc("Options");
        desc.add_options()("help,h", "Print help messages")("unsteg,u", po::bool_switch(&unsteg),
                                                            "unsteg message")(
            "input,i", po::value(&input_file),
            "Input file")("output,o", po::value(&output_file),
                          "output file")("password,p", po::value(&password), "encryption password");
        po::variables_map vm;

        try {
            po::store(po::parse_command_line(argc, argv, desc), vm);

            if (vm.count("help") || vm.count("h") || argc == 1) {
                help(desc);
                return 0;
            }

            po::notify(vm);

            if (unsteg)
                unsteg_data(password, input_file, output_file);
            else
                steg_data(password, input_file, output_file);
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
