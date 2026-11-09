#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "image_io.h"

bool write_png(const char* path, int w, int h, const uint8_t* rgba) {
  return stbi_write_png(path, w, h, 4, rgba, w * 4) != 0;
}
