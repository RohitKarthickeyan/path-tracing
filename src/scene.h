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
struct Hit { float t; Vec3 p, n; bool front; int mat; };

struct SceneView {
  const Sphere* spheres;
  int n_spheres;
  const Plane* planes;
  int n_planes;
  const Material* mats;
  Vec3 sky;
};

enum ViewMode : uint8_t { VIEW_COLOR, VIEW_NORMAL, VIEW_ALBEDO, VIEW_DEPTH };

struct Settings {
  float exposure = 1;
  bool tonemap_aces = true;
  ViewMode view = VIEW_COLOR;
};

struct Scene {
  std::vector<Sphere> spheres;
  std::vector<Plane> planes;
  std::vector<Material> mats;
  Vec3 sky{0, 0, 0};
  Camera cam{};
};
