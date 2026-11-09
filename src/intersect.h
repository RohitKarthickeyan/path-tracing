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

HD inline bool hit_triangle(const Triangle& tri, const Ray& r, float t_max, Hit& h) {
  Vec3 e1 = tri.v1 - tri.v0, e2 = tri.v2 - tri.v0, p = cross(r.d, e2);
  float det = dot(e1, p);
  if (fabsf(det) < 1e-12f) return false;
  float inv = 1 / det;
  Vec3 s = r.o - tri.v0;
  float u = dot(s, p) * inv;
  if (u < 0 || u > 1) return false;
  Vec3 q = cross(s, e1);
  float v = dot(r.d, q) * inv, t = dot(e2, q) * inv;
  if (v < 0 || u + v > 1 || t < T_MIN || t > t_max) return false;
  h.t = t;
  h.p = r.at(t);
  h.mat = tri.mat;
  set_normal(h, r.d, normalize(cross(e1, e2)));
  Vec3 shading = normalize((1 - u - v) * tri.n0 + u * tri.n1 + v * tri.n2);
  h.n = dot(shading, h.n) < 0 ? -shading : shading;
  return true;
}

HD inline bool hit_aabb(const Aabb& b, Vec3 o, Vec3 inv_d, float t_max) {
  float x0 = (b.lo.x - o.x) * inv_d.x, x1 = (b.hi.x - o.x) * inv_d.x;
  float y0 = (b.lo.y - o.y) * inv_d.y, y1 = (b.hi.y - o.y) * inv_d.y;
  float z0 = (b.lo.z - o.z) * inv_d.z, z1 = (b.hi.z - o.z) * inv_d.z;
  float t_in = fmaxf(fmaxf(fminf(x0, x1), fminf(y0, y1)), fminf(z0, z1));
  float t_out = fminf(fminf(fmaxf(x0, x1), fmaxf(y0, y1)), fmaxf(z0, z1));
  return t_out >= fmaxf(t_in, 0.0f) && t_in < t_max;
}

constexpr int BVH_STACK = 64;

// ponytail: children pushed in fixed order; nearest-first ordering if traversal is the bottleneck
HD inline bool hit_bvh(const SceneView& s, const Ray& r, float t_max, Hit& h) {
  if (s.n_tris == 0) return false;
  Vec3 inv{1 / r.d.x, 1 / r.d.y, 1 / r.d.z};
  int stack[BVH_STACK], sp = 0;
  stack[sp++] = 0;
  bool hit = false;
  while (sp > 0) {
    const BvhNode& n = s.nodes[stack[--sp]];
    if (!hit_aabb(n.box, r.o, inv, t_max)) continue;
    if (n.count > 0) {
      for (int i = n.left_or_first; i < n.left_or_first + n.count; i++)
        if (hit_triangle(s.tris[i], r, t_max, h)) hit = true, t_max = h.t;
    } else {
      stack[sp++] = n.left_or_first;
      stack[sp++] = n.left_or_first + 1;
    }
  }
  return hit;
}

HD inline bool hit_scene(const SceneView& s, const Ray& r, Hit& h) {
  float t_max = 1e30f;
  bool hit = false;
  for (int i = 0; i < s.n_spheres; i++)
    if (hit_sphere(s.spheres[i], r, t_max, h)) hit = true, t_max = h.t;
  for (int i = 0; i < s.n_planes; i++)
    if (hit_plane(s.planes[i], r, t_max, h)) hit = true, t_max = h.t;
  if (hit_bvh(s, r, t_max, h)) hit = true;
  return hit;
}
