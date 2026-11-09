#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "image_io.h"
#include "render.h"
#include "scenes.h"

int main(int argc, char** argv) {
  const char *scene_name = "sphere", *out = nullptr;
  int spp = 64;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!std::strcmp(argv[i], "--scene")) scene_name = argv[i + 1];
    else if (!std::strcmp(argv[i], "--spp")) spp = std::atoi(argv[i + 1]);
    else if (!std::strcmp(argv[i], "--out")) out = argv[i + 1];
  }
  Scene scene;
  if (!out || spp < 1 || !make_scene(scene_name, scene)) {
    std::fprintf(stderr, "usage: pt [--scene sphere] [--spp N] --out <file.png>\n");
    return 2;
  }
  const int w = 1280, h = 720;
  if (!write_png(out, w, h, render_scene(scene, w, h, spp).data())) {
    std::fprintf(stderr, "failed to write %s\n", out);
    return 1;
  }
  std::printf("wrote %s\n", out);
  return 0;
}
