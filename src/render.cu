#include <cuda_runtime.h>
#include "intersect.h"
#include "render.h"
#include "sampling.h"

constexpr int MAX_BOUNCES = 8;
constexpr float MISS_DEPTH = 1e4f;

struct Renderer {
  Sphere* spheres;
  Plane* planes;
  Material* mats;
  SceneView view;
  float4* accum;
  float4* normal_depth;
  float4* albedo;
  int w, h, spp;
};

__device__ Vec3 saturate(Vec3 v) { return {fminf(fmaxf(v.x, 0.0f), 1.0f), fminf(fmaxf(v.y, 0.0f), 1.0f), fminf(fmaxf(v.z, 0.0f), 1.0f)}; }

__device__ void add(float4& a, Vec3 v, float w = 0) {
  a.x += v.x;
  a.y += v.y;
  a.z += v.z;
  a.w += w;
}

__global__ void path_kernel(Renderer r, Camera cam, uint32_t frame) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= r.w || y >= r.h) return;
  int i = y * r.w + x;
  const SceneView& s = r.view;
  Rng rng = rng_seed(i, frame);
  float u = (x + rng.next_float()) / r.w, v = 1 - (y + rng.next_float()) / r.h;
  Ray ray = camera_ray(cam, float(r.w) / r.h, u, v, rng.next_float(), rng.next_float());
  Vec3 color{0, 0, 0}, throughput{1, 1, 1};
  Vec3 aov_n{0, 0, 0}, aov_a = saturate(s.sky);
  float aov_d = MISS_DEPTH;
  for (int b = 0; b < MAX_BOUNCES; b++) {
    Hit hit;
    if (!hit_scene(s, ray, hit)) {
      color = color + throughput * s.sky;
      break;
    }
    Material m = s.mats[hit.mat];
    if (b == 0) {
      aov_n = hit.n;
      aov_d = hit.t;
      aov_a = m.type == EMISSIVE ? saturate(m.emission) : m.albedo;
    }
    if (m.type == EMISSIVE) {
      color = color + throughput * m.emission;
      break;
    }
    Vec3 dir;
    if (m.type == DIFFUSE) {
      dir = cosine_direction(hit.n, rng);
    } else if (m.type == METAL) {
      dir = normalize(reflect(ray.d, hit.n) + m.fuzz * random_unit_vector(rng));
      if (dot(dir, hit.n) <= 0) break;
    } else {
      float eta = hit.front ? 1 / m.ior : m.ior;
      if (!refract(ray.d, hit.n, eta, dir) || rng.next_float() < schlick(-dot(ray.d, hit.n), eta))
        dir = reflect(ray.d, hit.n);
    }
    throughput = throughput * m.albedo;
    ray = {hit.p + hit.n * (dot(dir, hit.n) > 0 ? T_MIN : -T_MIN), dir};
  }
  add(r.normal_depth[i], aov_n, aov_d);
  add(r.albedo[i], aov_a);
  if (isfinite(color.x) && isfinite(color.y) && isfinite(color.z)) add(r.accum[i], color);
}

__device__ uint8_t unit_byte(float x) { return uint8_t(fminf(fmaxf(x, 0.0f), 1.0f) * 255 + 0.5f); }

__global__ void resolve_kernel(Renderer r, Settings st, uchar4* out) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= r.w || y >= r.h) return;
  int i = y * r.w + x;
  float inv = 1.0f / r.spp;
  if (st.view == VIEW_NORMAL) {
    float4 n = r.normal_depth[i];
    out[i] = make_uchar4(unit_byte(n.x * inv * 0.5f + 0.5f), unit_byte(n.y * inv * 0.5f + 0.5f), unit_byte(n.z * inv * 0.5f + 0.5f), 255);
  } else if (st.view == VIEW_ALBEDO) {
    float4 a = r.albedo[i];
    out[i] = make_uchar4(encode(a.x, inv, false), encode(a.y, inv, false), encode(a.z, inv, false), 255);
  } else if (st.view == VIEW_DEPTH) {
    uint8_t d = unit_byte(1 / (1 + 0.1f * r.normal_depth[i].w * inv));
    out[i] = make_uchar4(d, d, d, 255);
  } else {
    float4 a = r.accum[i];
    float e = st.exposure * inv;
    out[i] = make_uchar4(encode(a.x, e, st.tonemap_aces), encode(a.y, e, st.tonemap_aces), encode(a.z, e, st.tonemap_aces), 255);
  }
}

template <class T> static T* upload(const std::vector<T>& v) {
  T* d = nullptr;
  cudaMalloc(&d, v.size() * sizeof(T));
  cudaMemcpy(d, v.data(), v.size() * sizeof(T), cudaMemcpyHostToDevice);
  return d;
}

static dim3 grid_for(const Renderer* r) { return dim3((r->w + 15) / 16, (r->h + 15) / 16); }

Renderer* create_renderer(const Scene& scene, int w, int h) {
  Renderer* r = new Renderer{};
  r->spheres = upload(scene.spheres);
  r->planes = upload(scene.planes);
  r->mats = upload(scene.mats);
  r->view = {r->spheres, int(scene.spheres.size()), r->planes, int(scene.planes.size()), r->mats, scene.sky};
  r->w = w;
  r->h = h;
  size_t bytes = size_t(w) * h * sizeof(float4);
  cudaMalloc(&r->accum, bytes);
  cudaMalloc(&r->normal_depth, bytes);
  cudaMalloc(&r->albedo, bytes);
  reset_accumulation(r);
  return r;
}

void destroy_renderer(Renderer* r) {
  for (void* p : {(void*)r->spheres, (void*)r->planes, (void*)r->mats, (void*)r->accum, (void*)r->normal_depth, (void*)r->albedo}) cudaFree(p);
  delete r;
}

void reset_accumulation(Renderer* r) {
  size_t bytes = size_t(r->w) * r->h * sizeof(float4);
  cudaMemset(r->accum, 0, bytes);
  cudaMemset(r->normal_depth, 0, bytes);
  cudaMemset(r->albedo, 0, bytes);
  r->spp = 0;
}

void accumulate(Renderer* r, const Camera& cam, int spp) {
  for (int f = 0; f < spp; f++) path_kernel<<<grid_for(r), dim3(16, 16)>>>(*r, cam, r->spp + f);
  r->spp += spp;
}

void resolve(Renderer* r, const Settings& settings, void* device_rgba) {
  resolve_kernel<<<grid_for(r), dim3(16, 16)>>>(*r, settings, (uchar4*)device_rgba);
}

int accumulated_spp(const Renderer* r) { return r->spp; }

std::vector<uint8_t> render_scene(const Scene& scene, const Settings& settings, int w, int h, int spp) {
  Renderer* r = create_renderer(scene, w, h);
  accumulate(r, scene.cam, spp);
  uchar4* out = nullptr;
  cudaMalloc(&out, size_t(w) * h * sizeof(uchar4));
  resolve(r, settings, out);
  std::vector<uint8_t> px(size_t(w) * h * 4);
  cudaMemcpy(px.data(), out, px.size(), cudaMemcpyDeviceToHost);
  cudaFree(out);
  destroy_renderer(r);
  return px;
}

std::vector<float> render_linear(const Scene& scene, int w, int h, int spp) {
  Renderer* r = create_renderer(scene, w, h);
  accumulate(r, scene.cam, spp);
  std::vector<float4> sums(size_t(w) * h);
  cudaMemcpy(sums.data(), r->accum, sums.size() * sizeof(float4), cudaMemcpyDeviceToHost);
  destroy_renderer(r);
  std::vector<float> rgb;
  rgb.reserve(sums.size() * 3);
  for (float4 a : sums) rgb.insert(rgb.end(), {a.x / spp, a.y / spp, a.z / spp});
  return rgb;
}
