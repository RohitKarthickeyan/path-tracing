#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "image_io.h"

bool write_png(const char* path, int w, int h, const uint8_t* rgba) {
  return stbi_write_png(path, w, h, 4, rgba, w * 4) != 0;
}

bool load_hdr(const char* path, int& w, int& h, std::vector<float>& rgb) {
  int channels;
  float* data = stbi_loadf(path, &w, &h, &channels, 3);
  if (!data) return false;
  rgb.assign(data, data + size_t(w) * h * 3);
  stbi_image_free(data);
  return true;
}
