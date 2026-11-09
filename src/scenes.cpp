#include "scenes.h"

bool make_scene(const std::string& name, Scene& out) {
  if (name != "sphere") return false;
  out = {};
  out.mats = {
      {DIFFUSE, {0.5f, 0.5f, 0.5f}, {}},
      {DIFFUSE, {0.8f, 0.3f, 0.3f}, {}},
      {EMISSIVE, {}, {12, 11, 10}},
  };
  out.planes = {{{0, 0, 0}, {0, 1, 0}, 0}};
  out.spheres = {{{0, 1, 0}, 1, 1}, {{-3, 5, 2}, 1, 2}};
  out.sky = {0.15f, 0.2f, 0.3f};
  out.cam = {{0, 1.5f, 5}, 0, -0.1f, 45};
  return true;
}
