#pragma once
#include <vector>
#include "camera.h"

enum MatType : uint8_t { DIFFUSE, METAL, GLASS, EMISSIVE };

struct Material {
  MatType type;
  Vec3 albedo, emission;
  float fuzz, ior;
};

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
};

enum ViewMode : uint8_t { VIEW_COLOR, VIEW_NORMAL, VIEW_ALBEDO, VIEW_DEPTH };

struct Settings {
  float exposure = 1;
  bool tonemap_aces = true;
  ViewMode view = VIEW_COLOR;
  float env_intensity = 1, env_rotation = 0;  // rotation: radians about +Y
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
