#pragma once
#include <cstdint>
#include <vector>

bool write_png(const char* path, int w, int h, const uint8_t* rgba);
bool load_hdr(const char* path, int& w, int& h, std::vector<float>& rgb);
