#pragma once
#include "scene.h"

struct GSample { Vec3 n, albedo; float depth; };  // n unit length

HD inline float atrous_weight(const GSample& p, const GSample& q, const Settings& st) {
  float wn = powf(fmaxf(0.0f, dot(p.n, q.n)), st.sigma_n);
  Vec3 da = p.albedo - q.albedo;
  float wa = expf(-dot(da, da) / (st.sigma_a * st.sigma_a));
  float wz = expf(-fabsf(p.depth - q.depth) / (st.sigma_z * p.depth + 1e-4f));
  return wn * wa * wz;
}

// noise in r shrinks as 1/sqrt(spp), so r^2 * spp keeps noise weight constant while real edges sharpen
HD inline float color_weight(float lp, float lq, int spp, float sigma_c) {
  float r = fabsf(lp - lq) / (lp + lq + 1e-3f);
  return expf(-r * r * spp / (sigma_c * sigma_c));
}

// 3x3 Karis average (1 / (1 + luma) weights) so lone fireflies don't bloom, then soft-knee threshold
HD inline Vec3 bloom_bright(const Vec3 px[9], float threshold, float knee) {
  Vec3 sum{0, 0, 0};
  float wsum = 0;
  for (int i = 0; i < 9; i++) {
    float w = 1 / (1 + luminance(px[i]));
    sum = sum + w * px[i];
    wsum += w;
  }
  Vec3 c = sum / wsum;
  float l = luminance(c), soft = fminf(fmaxf(l - threshold + knee, 0.0f), 2 * knee);
  soft = soft * soft / (4 * knee + 1e-4f);
  return c * (fmaxf(soft, l - threshold) / fmaxf(l, 1e-4f));
}

// Device buffers are float4 per pixel, passed as float*.
float* denoise(float* a, float* b, const float* normal_depth, const float* albedo, int w, int h, int spp, const Settings& st);
void bloom(const float* in, float* out, float* tmp, int w, int h, const Settings& st);
