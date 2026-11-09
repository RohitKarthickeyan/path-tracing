#include "scenes.h"

bool make_scene(const std::string& name, Scene& out) {
  out = {};
  if (name == "furnace") {
    out.mats = {{DIFFUSE, {0.8f, 0.8f, 0.8f}, {}}};
    out.spheres = {{{0, 0, 0}, 1, 0}};
    out.sky = {1, 1, 1};
    out.cam = {{0, 0, 3}, 0, 0, 45};
    return true;
  }
  if (name == "cornell") {
    out.mats = {
        {DIFFUSE, {0.73f, 0.73f, 0.73f}, {}},
        {DIFFUSE, {0.65f, 0.05f, 0.05f}, {}},
        {DIFFUSE, {0.12f, 0.45f, 0.15f}, {}},
        {EMISSIVE, {}, {15, 15, 15}},
    };
    out.planes = {
        {{0, 0, 0}, {0, 1, 0}, 0},
        {{0, 2, 0}, {0, -1, 0}, 0},
        {{0, 0, -1}, {0, 0, 1}, 0},
        {{-1, 0, 0}, {1, 0, 0}, 1},
        {{1, 0, 0}, {-1, 0, 0}, 2},
    };
    out.spheres = {{{0, 2, -0.2f}, 0.35f, 3}, {{-0.4f, 0.35f, -0.4f}, 0.35f, 0}, {{0.45f, 0.3f, 0.1f}, 0.3f, 0}};
    out.cam = {{0, 1, 3.4f}, 0, 0, 40};
    return true;
  }
  if (name != "sphere") return false;
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
