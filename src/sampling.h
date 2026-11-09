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

constexpr float GAMMA = 2.2f;

// Narkowicz 2015 ACES fit
HD inline float aces(float x) {
  return fminf(fmaxf(x * (2.51f * x + 0.03f) / (x * (2.43f * x + 0.59f) + 0.14f), 0.0f), 1.0f);
}

HD inline uint8_t encode(float linear, float exposure, bool tonemap_aces) {
  float x = linear * exposure;
  x = tonemap_aces ? aces(x) : fminf(x, 1.0f);
  return uint8_t(powf(fmaxf(x, 0.0f), 1 / GAMMA) * 255 + 0.5f);
}

HD inline float schlick(float cos_i, float eta) {
  float r0 = (1 - eta) / (1 + eta);
  r0 *= r0;
  float m = 1 - cos_i;
  return r0 + (1 - r0) * m * m * m * m * m;
}
