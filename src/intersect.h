#pragma once
#include "scene.h"

constexpr float T_MIN = 1e-4f;

HD inline void set_normal(Hit& h, Vec3 d, Vec3 outward) {
  h.front = dot(d, outward) < 0;
  h.n = h.front ? outward : -outward;
}

HD inline bool hit_sphere(const Sphere& s, const Ray& r, float t_max, Hit& h) {
  Vec3 oc = r.o - s.c;
  float b = dot(oc, r.d), c = dot(oc, oc) - s.r * s.r, disc = b * b - c;
  if (disc < 0) return false;
  float sq = sqrtf(disc), t = -b - sq;
  if (t < T_MIN) t = -b + sq;
  if (t < T_MIN || t > t_max) return false;
  h.t = t;
  h.p = r.at(t);
  h.mat = s.mat;
  set_normal(h, r.d, (h.p - s.c) / s.r);
  return true;
}

HD inline bool hit_plane(const Plane& p, const Ray& r, float t_max, Hit& h) {
  float denom = dot(p.n, r.d);
  if (fabsf(denom) < 1e-8f) return false;
  float t = dot(p.p - r.o, p.n) / denom;
  if (t < T_MIN || t > t_max) return false;
  h.t = t;
  h.p = r.at(t);
  h.mat = p.mat;
  set_normal(h, r.d, p.n);
  return true;
}

HD inline bool hit_scene(const SceneView& s, const Ray& r, Hit& h) {
  float t_max = 1e30f;
  bool hit = false;
  for (int i = 0; i < s.n_spheres; i++)
    if (hit_sphere(s.spheres[i], r, t_max, h)) hit = true, t_max = h.t;
  for (int i = 0; i < s.n_planes; i++)
    if (hit_plane(s.planes[i], r, t_max, h)) hit = true, t_max = h.t;
  return hit;
}
