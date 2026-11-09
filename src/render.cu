#include <cuda_runtime.h>
#include "intersect.h"
#include "render.h"
#include "sampling.h"

constexpr int MAX_BOUNCES = 8;
constexpr float GAMMA = 2.2f;

__global__ void path_kernel(float4* accum, SceneView s, Camera cam, int w, int h, uint32_t frame) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  int i = y * w + x;
  Rng rng = rng_seed(i, frame);
  Ray ray = camera_ray(cam, float(w) / h, (x + rng.next_float()) / w, 1 - (y + rng.next_float()) / h);
  Vec3 color{0, 0, 0}, throughput{1, 1, 1};
  for (int b = 0; b < MAX_BOUNCES; b++) {
    Hit hit;
    if (!hit_scene(s, ray, hit)) {
      color = color + throughput * s.sky;
      break;
    }
    Material m = s.mats[hit.mat];
    if (m.type == EMISSIVE) {
      color = color + throughput * m.emission;
      break;
    }
    throughput = throughput * m.albedo;
    ray = {hit.p + hit.n * T_MIN, cosine_direction(hit.n, rng)};
  }
  accum[i].x += color.x;
  accum[i].y += color.y;
  accum[i].z += color.z;
}

__device__ uint8_t encode(float sum, int spp) { return uint8_t(fminf(powf(sum / spp, 1 / GAMMA), 1.0f) * 255 + 0.5f); }

__global__ void resolve_kernel(const float4* accum, uchar4* out, int w, int h, int spp) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  float4 a = accum[y * w + x];
  out[y * w + x] = make_uchar4(encode(a.x, spp), encode(a.y, spp), encode(a.z, spp), 255);
}

template <class T> static T* upload(const std::vector<T>& v) {
  T* d = nullptr;
  cudaMalloc(&d, v.size() * sizeof(T));
  cudaMemcpy(d, v.data(), v.size() * sizeof(T), cudaMemcpyHostToDevice);
  return d;
}

std::vector<uint8_t> render_scene(const Scene& scene, int w, int h, int spp) {
  Sphere* spheres = upload(scene.spheres);
  Plane* planes = upload(scene.planes);
  Material* mats = upload(scene.mats);
  SceneView s{spheres, int(scene.spheres.size()), planes, int(scene.planes.size()), mats, scene.sky};
  size_t n = size_t(w) * h;
  float4* accum = nullptr;
  uchar4* out = nullptr;
  cudaMalloc(&accum, n * sizeof(float4));
  cudaMemset(accum, 0, n * sizeof(float4));
  cudaMalloc(&out, n * sizeof(uchar4));
  dim3 block(16, 16), grid((w + 15) / 16, (h + 15) / 16);
  for (int f = 0; f < spp; f++) path_kernel<<<grid, block>>>(accum, s, scene.cam, w, h, f);
  resolve_kernel<<<grid, block>>>(accum, out, w, h, spp);
  std::vector<uint8_t> px(n * 4);
  cudaMemcpy(px.data(), out, px.size(), cudaMemcpyDeviceToHost);
  cudaFree(spheres);
  cudaFree(planes);
  cudaFree(mats);
  cudaFree(accum);
  cudaFree(out);
  return px;
}
