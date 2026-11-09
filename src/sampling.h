#pragma once
#include "hd.h"

// n + uniform unit-sphere point is cosine-distributed about n
HD inline Vec3 cosine_direction(Vec3 n, Rng& rng) {
  float z = 1 - 2 * rng.next_float(), phi = 2 * PI * rng.next_float();
  float r = sqrtf(fmaxf(0.0f, 1 - z * z));
  Vec3 d = n + Vec3{r * cosf(phi), r * sinf(phi), z};
  float len = length(d);
  return len > 1e-6f ? d / len : n;
}
