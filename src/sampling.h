#pragma once
#include <vector>
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

HD inline Vec3 rotate_y(Vec3 d, float a) {
  float c = cosf(a), s = sinf(a);
  return {c * d.x + s * d.z, d.y, -s * d.x + c * d.z};
}

HD inline void env_uv(Vec3 d, float rotation, float& u, float& v) {
  Vec3 r = rotate_y(d, rotation);
  u = 0.5f + atan2f(r.x, -r.z) / (2 * PI);
  v = acosf(fminf(fmaxf(r.y, -1.0f), 1.0f)) / PI;
}

HD inline int env_index(float u, float v, int w, int h) {
  int x = int(u * w), y = int(v * h);
  return (y < h ? y : h - 1) * w + (x < w ? x : w - 1);
}

HD inline Vec3 env_lookup(const float* env, int w, int h, Vec3 d, float rotation) {
  float u, v;
  env_uv(d, rotation, u, v);
  const float* p = env + 3 * env_index(u, v, w, h);
  return {p[0], p[1], p[2]};
}

// CDFs over env pixels weighted by luminance * sin(theta); marginal over rows, conditional per row
inline void build_env_cdf(const float* env, int w, int h, std::vector<float>& marginal, std::vector<float>& conditional) {
  marginal.assign(h, 0);
  conditional.assign(size_t(w) * h, 0);
  float total = 0;
  for (int y = 0; y < h; y++) {
    float s = sinf(PI * (y + 0.5f) / h), row = 0;
    float* c = &conditional[size_t(y) * w];
    for (int x = 0; x < w; x++) {
      const float* p = env + 3 * (size_t(y) * w + x);
      row += luminance({p[0], p[1], p[2]}) * s;
      c[x] = row;
    }
    for (int x = 0; x < w; x++) c[x] = row > 0 ? c[x] / row : float(x + 1) / w;
    total += row;
    marginal[y] = total;
  }
  if (total <= 0) marginal.clear();
  for (float& m : marginal) m /= total;
}

struct EnvDist { const float* marginal; const float* conditional; int w, h; };

HD inline int find_cdf(const float* cdf, int n, float u) {
  int lo = 0, hi = n - 1;
  while (lo < hi) {
    int mid = (lo + hi) / 2;
    if (cdf[mid] > u) hi = mid;
    else lo = mid + 1;
  }
  return lo;
}

HD inline float cdf_step(const float* cdf, int i) { return cdf[i] - (i > 0 ? cdf[i - 1] : 0); }

// solid-angle pdf = pixel pdf * w * h / (2 pi^2 sin(theta))
HD inline float env_pixel_pdf(const EnvDist& e, int x, int y, float sin_theta) {
  if (sin_theta <= 0) return 0;
  return cdf_step(e.marginal, y) * cdf_step(e.conditional + y * e.w, x) * e.w * e.h / (2 * PI * PI * sin_theta);
}

HD inline float env_pdf(const EnvDist& e, Vec3 d, float rotation) {
  float u, v;
  env_uv(d, rotation, u, v);
  int i = env_index(u, v, e.w, e.h);
  return env_pixel_pdf(e, i % e.w, i / e.w, sinf(v * PI));
}

HD inline Vec3 env_sample(const EnvDist& e, float rotation, Rng& rng, float& pdf) {
  int y = find_cdf(e.marginal, e.h, rng.next_float());
  int x = find_cdf(e.conditional + y * e.w, e.w, rng.next_float());
  float theta = (y + rng.next_float()) / e.h * PI, phi = ((x + rng.next_float()) / e.w - 0.5f) * 2 * PI;
  float st = sinf(theta);
  pdf = env_pixel_pdf(e, x, y, st);
  return rotate_y({st * sinf(phi), cosf(theta), -st * cosf(phi)}, -rotation);
}

HD inline float schlick(float cos_i, float eta) {
  float r0 = (1 - eta) / (1 + eta);
  r0 *= r0;
  float m = 1 - cos_i;
  return r0 + (1 - r0) * m * m * m * m * m;
}
