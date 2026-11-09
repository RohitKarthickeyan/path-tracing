#pragma once
#include "hd.h"

struct Camera {
  Vec3 pos;
  float yaw, pitch, vfov;  // radians, radians, degrees; yaw 0 looks down -Z
};

HD inline Ray camera_ray(const Camera& c, float aspect, float u, float v) {
  Vec3 fwd{cosf(c.pitch) * sinf(c.yaw), sinf(c.pitch), -cosf(c.pitch) * cosf(c.yaw)};
  Vec3 right = normalize(cross(fwd, Vec3{0, 1, 0}));
  Vec3 up = cross(right, fwd);
  float h = tanf(c.vfov * PI / 360);
  return {c.pos, normalize(fwd + (2 * u - 1) * h * aspect * right + (2 * v - 1) * h * up)};
}
