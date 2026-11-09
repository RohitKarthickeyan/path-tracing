#pragma once
#include "hd.h"

HD inline Vec3 random_unit_vector(Rng& rng) {
  float z = 1 - 2 * rng.next_float(), phi = 2 * PI * rng.next_float();
  float r = sqrtf(fmaxf(0.0f, 1 - z * z));
  return {r * cosf(phi), r * sinf(phi), z};
}

// n + uniform unit-sphere point is cosine-distributed about n
HD inline Vec3 cosine_direction(Vec3 n, Rng& rng) {
  Vec3 d = n + random_unit_vector(rng);
  float len = length(d);
  return len > 1e-6f ? d / len : n;
}

HD inline Vec3 reflect(Vec3 d, Vec3 n) { return d - 2 * dot(d, n) * n; }

// eta = n_from / n_to; n faces against d; false on total internal reflection
HD inline bool refract(Vec3 d, Vec3 n, float eta, Vec3& out) {
  float cos_i = -dot(d, n), sin2_t = eta * eta * (1 - cos_i * cos_i);
  if (sin2_t > 1) return false;
  out = eta * d + (eta * cos_i - sqrtf(1 - sin2_t)) * n;
  return true;
}

HD inline float schlick(float cos_i, float eta) {
  float r0 = (1 - eta) / (1 + eta);
  r0 *= r0;
  float m = 1 - cos_i;
  return r0 + (1 - r0) * m * m * m * m * m;
}
