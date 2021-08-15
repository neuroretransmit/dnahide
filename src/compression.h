#pragma once

#include <string>
#include <vector>

#include "types.h"

using namespace std;

void compress_memory(const string& data, vector<u8>& out_data);
size_t decompress_memory(const void* data, size_t data_size, vector<u8>& out_data);
