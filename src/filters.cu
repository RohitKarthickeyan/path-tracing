#include <cuda_runtime.h>
#include "filters.h"

__constant__ float H[5] = {1 / 16.0f, 1 / 4.0f, 3 / 8.0f, 1 / 4.0f, 1 / 16.0f};

__device__ Vec3 rgb(float4 v) { return {v.x, v.y, v.z}; }

__device__ GSample gsample(const float4* nd, const float4* alb, int i, float inv) {
  float4 n = nd[i];
  Vec3 dir = rgb(n);
  float len = length(dir);
  return {len > 0 ? dir / len : dir, rgb(alb[i]) * inv, n.w * inv};
}

__global__ void atrous_kernel(const float4* in, float4* out, const float4* nd, const float4* alb, int w, int h, float inv,
                              int spp, int step, Settings st) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  int i = y * w + x;
  GSample p = gsample(nd, alb, i, inv);
  float lp = luminance(rgb(in[i]));
  if (p.n.x == 0 && p.n.y == 0 && p.n.z == 0) {
    out[i] = in[i];
    return;
  }
  Vec3 sum{0, 0, 0};
  float wsum = 0;
  for (int dy = -2; dy <= 2; dy++) {
    for (int dx = -2; dx <= 2; dx++) {
      int qx = x + dx * step, qy = y + dy * step;
      if (qx < 0 || qy < 0 || qx >= w || qy >= h) continue;
      int q = qy * w + qx;
      float wt = H[dx + 2] * H[dy + 2] * atrous_weight(p, gsample(nd, alb, q, inv), st) *
                 color_weight(lp, luminance(rgb(in[q])), spp, st.sigma_c);
      sum = sum + wt * rgb(in[q]);
      wsum += wt;
    }
  }
  Vec3 c = sum / wsum;
  out[i] = make_float4(c.x, c.y, c.z, 0);
}

__global__ void bright_kernel(const float4* in, float4* out, int w, int h, float threshold, float knee) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  Vec3 px[9];
  for (int k = 0; k < 9; k++) {
    int qx = min(max(x + k % 3 - 1, 0), w - 1), qy = min(max(y + k / 3 - 1, 0), h - 1);
    px[k] = rgb(in[qy * w + qx]);
  }
  Vec3 c = bloom_bright(px, threshold, knee);
  out[y * w + x] = make_float4(c.x, c.y, c.z, 0);
}

__global__ void blur_kernel(const float4* in, float4* out, int w, int h, int dx, int dy, int radius) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  float sigma = radius / 3.0f;
  Vec3 sum{0, 0, 0};
  float wsum = 0;
  for (int k = -radius; k <= radius; k++) {
    int qx = min(max(x + k * dx, 0), w - 1), qy = min(max(y + k * dy, 0), h - 1);
    float wt = expf(-k * k / (2 * sigma * sigma));
    sum = sum + wt * rgb(in[qy * w + qx]);
    wsum += wt;
  }
  Vec3 c = sum / wsum;
  out[y * w + x] = make_float4(c.x, c.y, c.z, 0);
}

static dim3 grid(int w, int h) { return dim3((w + 15) / 16, (h + 15) / 16); }

float* denoise(float* a, float* b, const float* normal_depth, const float* albedo, int w, int h, int spp, const Settings& st) {
  for (int i = 0; i < st.denoise_iters; i++) {
    atrous_kernel<<<grid(w, h), dim3(16, 16)>>>((float4*)a, (float4*)b, (const float4*)normal_depth, (const float4*)albedo, w, h,
                                                1.0f / spp, spp, 1 << i, st);
    float* t = a;
    a = b;
    b = t;
  }
  return a;
}

void bloom(const float* in, float* out, float* tmp, int w, int h, const Settings& st) {
  bright_kernel<<<grid(w, h), dim3(16, 16)>>>((const float4*)in, (float4*)out, w, h, st.bloom_threshold, st.bloom_knee);
  blur_kernel<<<grid(w, h), dim3(16, 16)>>>((float4*)out, (float4*)tmp, w, h, 1, 0, st.bloom_radius);
  blur_kernel<<<grid(w, h), dim3(16, 16)>>>((float4*)tmp, (float4*)out, w, h, 0, 1, st.bloom_radius);
}
