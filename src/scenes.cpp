#include "bvh.h"
#include "image_io.h"
#include "obj.h"
#include "scenes.h"

static void place(std::vector<Triangle> tris, Vec3 base, float height, std::vector<Triangle>& out) {
  Aabb b{tris[0].v0, tris[0].v0};
  for (const Triangle& t : tris) b = {vmin(vmin(b.lo, t.v0), vmin(t.v1, t.v2)), vmax(vmax(b.hi, t.v0), vmax(t.v1, t.v2))};
  float s = height / (b.hi.y - b.lo.y);
  Vec3 c{(b.lo.x + b.hi.x) / 2, b.lo.y, (b.lo.z + b.hi.z) / 2};
  for (Triangle t : tris) {
    t.v0 = (t.v0 - c) * s + base;
    t.v1 = (t.v1 - c) * s + base;
    t.v2 = (t.v2 - c) * s + base;
    out.push_back(t);
  }
}

static bool bunnies(Scene& out) {
  out.mats = {
      {DIFFUSE, {0.5f, 0.5f, 0.5f}},
      {GLASS, {1, 1, 1}, {}, 0, 1.5f},
      {METAL, {1.0f, 0.71f, 0.29f}, {}, 0.05f},
  };
  std::vector<Triangle> bunny;
  if (!load_obj("assets/bunny.obj", 1, bunny)) return false;
  place(bunny, {-0.65f, 0, 0}, 1, out.tris);
  for (Triangle& t : bunny) t.mat = 2;
  place(bunny, {0.65f, 0, 0}, 1, out.tris);
  build_bvh(out.tris, out.bvh);
  out.planes = {{{0, 0, 0}, {0, 1, 0}, 0}};
  if (!load_hdr("assets/sky.hdr", out.env_w, out.env_h, out.env)) return false;
  out.cam = {{0, 0.9f, 3.2f}, 0, -0.12f, 35};
  return true;
}

static Camera look_at(Vec3 pos, Vec3 target, float vfov, float aperture) {
  Vec3 d = target - pos;
  float dist = length(d);
  d = d / dist;
  return {pos, atan2f(d.x, -d.z), asinf(d.y), vfov, aperture, dist};
}

static void random_spheres(Scene& out) {
  out.mats = {
      {DIFFUSE, {0.5f, 0.5f, 0.5f}},
      {GLASS, {1, 1, 1}, {}, 0, 1.5f},
      {DIFFUSE, {0.4f, 0.2f, 0.1f}},
      {METAL, {0.7f, 0.6f, 0.5f}, {}, 0},
  };
  out.planes = {{{0, 0, 0}, {0, 1, 0}, 0}};
  out.spheres = {{{0, 1, 0}, 1, 1}, {{-4, 1, 0}, 1, 2}, {{4, 1, 0}, 1, 3}};
  Rng rng = rng_seed(42, 0);
  for (int a = -11; a < 11; a++) {
    for (int b = -11; b < 11; b++) {
      Vec3 c{a + 0.9f * rng.next_float(), 0.2f, b + 0.9f * rng.next_float()};
      if (length(c - Vec3{4, 0.2f, 0}) < 0.9f || length(c - Vec3{-4, 0.2f, 0}) < 0.9f || length(c - Vec3{0, 0.2f, 0}) < 0.9f)
        continue;
      float pick = rng.next_float();
      Vec3 col{rng.next_float(), rng.next_float(), rng.next_float()};
      if (pick < 0.8f) out.mats.push_back({DIFFUSE, col * col});
      else if (pick < 0.95f) out.mats.push_back({METAL, 0.5f * (col + Vec3{1, 1, 1}), {}, 0.5f * rng.next_float()});
      else out.mats.push_back({GLASS, {1, 1, 1}, {}, 0, 1.5f});
      out.spheres.push_back({c, 0.2f, int(out.mats.size()) - 1});
    }
  }
  out.sky = {0.7f, 0.8f, 1.0f};
  out.cam = look_at({13, 2, 3}, {0, 1, 0}, 20, 0.25f);
}

bool make_scene(const std::string& name, Scene& out) {
  out = {};
  if (name == "spheres") {
    random_spheres(out);
    return true;
  }
  if (name == "bunny") return bunnies(out);
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
