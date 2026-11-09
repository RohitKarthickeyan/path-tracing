#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "image_io.h"
#include "render.h"
#include "scenes.h"
#include "viewer.h"

int main(int argc, char** argv) {
  const char *scene_name = "sphere", *out = nullptr;
  int spp = 64;
  Settings settings;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (!std::strcmp(argv[i], "--scene")) scene_name = argv[i + 1];
    else if (!std::strcmp(argv[i], "--spp")) spp = std::atoi(argv[i + 1]);
    else if (!std::strcmp(argv[i], "--exposure")) settings.exposure = float(std::atof(argv[i + 1]));
    else if (!std::strcmp(argv[i], "--out")) out = argv[i + 1];
  }
  Scene scene;
  if (spp < 1 || settings.exposure <= 0 || !make_scene(scene_name, scene)) {
    std::fprintf(stderr, "usage: pt [--scene sphere|furnace|cornell|spheres|bunny] [--spp N] [--exposure X] [--out <file.png>]\n");
    return 2;
  }
  const int w = 1280, h = 720;
  if (!out) {
    run_viewer(scene, settings, w, h);
    return 0;
  }
  if (!write_png(out, w, h, render_scene(scene, settings, w, h, spp).data())) {
    std::fprintf(stderr, "failed to write %s\n", out);
    return 1;
  }
  std::printf("wrote %s\n", out);
  return 0;
}
