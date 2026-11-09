#include <cstdio>
#include <cstring>
#include "image_io.h"
#include "render.h"

int main(int argc, char** argv) {
  const char* out = nullptr;
  for (int i = 1; i + 1 < argc; i++)
    if (!std::strcmp(argv[i], "--out")) out = argv[i + 1];
  if (!out) {
    std::fprintf(stderr, "usage: pt --out <file.png>\n");
    return 2;
  }
  const int w = 1280, h = 720;
  if (!write_png(out, w, h, render_gradient(w, h).data())) {
    std::fprintf(stderr, "failed to write %s\n", out);
    return 1;
  }
  std::printf("wrote %s\n", out);
  return 0;
}
