#pragma once
#include "hd.h"

struct Camera {
  Vec3 pos;
  float yaw, pitch, vfov;  // radians, radians, degrees; yaw 0 looks down -Z
  float aperture, focus_dist;  // aperture 0 = pinhole
};

HD inline Vec3 camera_forward(const Camera& c) {
  return {cosf(c.pitch) * sinf(c.yaw), sinf(c.pitch), -cosf(c.pitch) * cosf(c.yaw)};
}

HD inline Ray camera_ray(const Camera& c, float aspect, float u, float v, float lu, float lv) {
  Vec3 fwd = camera_forward(c);
  Vec3 right = normalize(cross(fwd, Vec3{0, 1, 0}));
  Vec3 up = cross(right, fwd);
  float h = tanf(c.vfov * PI / 360);
  Vec3 d = normalize(fwd + (2 * u - 1) * h * aspect * right + (2 * v - 1) * h * up);
  if (c.aperture == 0) return {c.pos, d};
  float r = 0.5f * c.aperture * sqrtf(lu), phi = 2 * PI * lv;
  Vec3 o = c.pos + r * cosf(phi) * right + r * sinf(phi) * up;
  Vec3 focus = c.pos + d * (c.focus_dist / dot(d, fwd));
  return {o, normalize(focus - o)};
}
