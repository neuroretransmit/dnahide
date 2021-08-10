#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
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

static void help(const po::options_description& desc)
{
    cout << "DNAHide - Hide messages in DNA" << endl << desc << endl;
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

static void compress_memory(void* data, size_t data_size, vector<uint8_t>& out_data)
{
    vector<uint8_t> buffer;

    const size_t BUFSIZE = 128 * 1024;
    uint8_t temp_buffer[BUFSIZE];

    z_stream strm;
    strm.zalloc = 0;
    strm.zfree = 0;
    strm.next_in = reinterpret_cast<uint8_t*>(data);
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

static void encrypt(vector<u8>& data, const string& password)
{
    cerr << "Encrypting data..." << endl;
    // Additional authenticated data is the SHA256 of input file
    vector<u8> aad = sha256((char*) data.data());
    // TODO: Use a key derivation function
    vector<u8> key_generating_key = sha256((char*) password.data());
    // Create AEAD using RC6
    AEAD<WordSize::BLOCK_128> aead(key_generating_key);
    aead.seal(data, aad);
}

__attribute__((unused)) static void decrypt(vector<u8>& data, const string& password)
{
    cerr << "Decrypting data..." << endl;
    // Additional authenticated data is the SHA256 of input file
    vector<u8> aad = sha256((char*) data.data());
    // TODO: Use a key derivation function
    vector<u8> key_generating_key = sha256((char*) password.data());
    // Create AEAD using RC6
    AEAD<WordSize::BLOCK_128> aead(key_generating_key);
    aead.open(data, aad);
}

static void steg_data(string password, string input_file, string output_file)
{
    string data = "";
    vector<u8> compressed;
    vector<u8> encrypted;
    string dna;
    ofstream ofs;

    if (input_file != "") {
        if (file_exists(input_file)) {
            data = read_file(input_file);
        } else {
            cerr << "ERROR: File '" << input_file << "' does not exist.\n";
            exit(ERROR_IN_COMMAND_LINE);
        }
    } else {
        cout << "<<< BEGIN MESSAGE (Press CTRL+D when done) >>>\n\n";
        for (string line; getline(cin, line);)
            data += line + "\n";
    }

    cerr << "Compressing..." << endl;
    compress_memory((void*) data.data(), data.size(), compressed);

    if (password != "")
        encrypt(encrypted = compressed, password);

    cerr << "Encoding DNA..." << endl;
    dna = dna64::encode((char*) (password != "" ? encrypted.data() : compressed.data()));

    if (output_file != "") {
        ofs = ofstream(output_file);
        ofs << dna << endl;
        ofs.close();
    } else {
        cout << dna << endl;
    }
}

static void unsteg_data(__attribute__((unused)) string password, string input_file,
                        __attribute__((unused)) string output_file)
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
        cout << "<<< BEGIN MESSAGE (Press CTRL+D when done) >>>\n\n";
        for (string line; getline(cin, line);)
            data += line + "\n";
    }

    data = dna64::decode(data);
    cout << data << endl;

    // Decode dna64
    // Decrypt
    // Decompress
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
                                                            "Unsteg message")(
            "input,i", po::value(&input_file),
            "Input file")("output,o", po::value(&output_file),
                          "Output file")("password,p", po::value(&password), "Encryption password");
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
