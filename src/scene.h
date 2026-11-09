#pragma once
#include <vector>
#include "camera.h"

enum MatType : uint8_t { DIFFUSE, METAL, GLASS, EMISSIVE };
enum Pattern : uint8_t { SOLID, CHECKER };  // CHECKER: squares in the xz plane

struct Material {
  MatType type;
  Vec3 albedo, emission;
  float fuzz, ior;
  Pattern pattern;
  Vec3 albedo2;
  float checker_scale;  // squares per unit
};

HD inline Vec3 material_albedo(const Material& m, Vec3 p) {
  if (m.pattern == SOLID) return m.albedo;
  int k = int(floorf(p.x * m.checker_scale)) + int(floorf(p.z * m.checker_scale));
  return k & 1 ? m.albedo2 : m.albedo;
}

struct Sphere { Vec3 c; float r; int mat; };
struct Plane { Vec3 p, n; int mat; };
struct Triangle { Vec3 v0, v1, v2, n0, n1, n2; int mat; };
struct BvhNode { Aabb box; int left_or_first, count; };  // count > 0: leaf; else children at left, left + 1
struct Hit { float t; Vec3 p, n; bool front; int mat; };

struct SceneView {
  const Sphere* spheres;
  int n_spheres;
  const Plane* planes;
  int n_planes;
  const Material* mats;
  Vec3 sky;
  const Triangle* tris;
  int n_tris;
  const BvhNode* nodes;
  const float* env;  // RGB equirect, null = constant sky
  int env_w, env_h;
  const float* env_marginal;  // null = no env importance sampling
  const float* env_conditional;
};

enum ViewMode : uint8_t { VIEW_COLOR, VIEW_NORMAL, VIEW_ALBEDO, VIEW_DEPTH };

struct Settings {
  float exposure = 1;
  bool tonemap_aces = true;
  ViewMode view = VIEW_COLOR;
  int spp_per_frame = 1, max_bounces = 8;
  float env_intensity = 1, env_rotation = 0;  // rotation: radians about +Y
  bool denoise = true;
  int denoise_iters = 5;
  float sigma_n = 128, sigma_a = 0.1f, sigma_z = 0.1f, sigma_c = 4;
  bool bloom = true;
  float bloom_threshold = 1, bloom_knee = 0.5f, bloom_strength = 0.05f;
  int bloom_radius = 16;  // pixels
};

struct Scene {
  std::vector<Sphere> spheres;
  std::vector<Plane> planes;
  std::vector<Triangle> tris;
  std::vector<BvhNode> bvh;
  std::vector<Material> mats;
  Vec3 sky{0, 0, 0};
  std::vector<float> env;
  int env_w = 0, env_h = 0;
  Camera cam{};
};
