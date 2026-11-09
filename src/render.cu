#include <cuda_runtime.h>
#include "filters.h"
#include "intersect.h"
#include "render.h"
#include "sampling.h"

constexpr float MISS_DEPTH = 1e4f;

struct Renderer {
  Sphere* spheres;
  Plane* planes;
  Material* mats;
  Triangle* tris;
  BvhNode* nodes;
  float* env;
  float *env_marginal, *env_conditional;
  SceneView view;
  float4* accum;
  float4* normal_depth;
  float4* albedo;
  float4 *ping, *pong, *glow, *glow_tmp;
  int w, h, spp;
};

__device__ Vec3 saturate(Vec3 v) { return {fminf(fmaxf(v.x, 0.0f), 1.0f), fminf(fmaxf(v.y, 0.0f), 1.0f), fminf(fmaxf(v.z, 0.0f), 1.0f)}; }

__device__ void add(float4& a, Vec3 v, float w = 0) {
  a.x += v.x;
  a.y += v.y;
  a.z += v.z;
  a.w += w;
}

__device__ Vec3 sky_radiance(const SceneView& s, const Settings& st, Vec3 d) {
  return s.env ? st.env_intensity * env_lookup(s.env, s.env_w, s.env_h, d, st.env_rotation) : s.sky;
}

__global__ void path_kernel(Renderer r, Camera cam, Settings st, uint32_t frame) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= r.w || y >= r.h) return;
  int i = y * r.w + x;
  const SceneView& s = r.view;
  Rng rng = rng_seed(i, frame);
  float u = (x + rng.next_float()) / r.w, v = 1 - (y + rng.next_float()) / r.h;
  Ray ray = camera_ray(cam, float(r.w) / r.h, u, v, rng.next_float(), rng.next_float());
  Vec3 color{0, 0, 0}, throughput{1, 1, 1};
  Vec3 aov_n{0, 0, 0}, aov_a = saturate(sky_radiance(s, st, ray.d));
  float aov_d = MISS_DEPTH;
  EnvDist ed{s.env_marginal, s.env_conditional, s.env_w, s.env_h};
  float bsdf_pdf = 0;  // cosine pdf of the last diffuse bounce; 0 after camera or specular
  for (int b = 0; b < st.max_bounces; b++) {
    Hit hit;
    if (!hit_scene(s, ray, hit)) {
      float mis = ed.marginal && bsdf_pdf > 0 ? bsdf_pdf / (bsdf_pdf + env_pdf(ed, ray.d, st.env_rotation)) : 1;
      color = color + throughput * sky_radiance(s, st, ray.d) * mis;
      break;
    }
    Material m = s.mats[hit.mat];
    Vec3 albedo = material_albedo(m, hit.p);
    if (b == 0) {
      aov_n = hit.n;
      aov_d = hit.t;
      aov_a = m.type == EMISSIVE ? saturate(m.emission) : albedo;
    }
    if (m.type == EMISSIVE) {
      color = color + throughput * m.emission;
      break;
    }
    Vec3 dir;
    bsdf_pdf = 0;
    if (m.type == DIFFUSE) {
      if (ed.marginal) {
        float light_pdf;
        Vec3 l = env_sample(ed, st.env_rotation, rng, light_pdf);
        float c = dot(l, hit.n);
        Hit blocker;
        if (c > 0 && light_pdf > 0 && !hit_scene(s, {hit.p + hit.n * T_MIN, l}, blocker))
          color = color + throughput * albedo * sky_radiance(s, st, l) * (c / PI / (light_pdf + c / PI));
      }
      dir = cosine_direction(hit.n, rng);
      bsdf_pdf = dot(dir, hit.n) / PI;
    } else if (m.type == METAL) {
      dir = normalize(reflect(ray.d, hit.n) + m.fuzz * random_unit_vector(rng));
      if (dot(dir, hit.n) <= 0) break;
    } else {
      float eta = hit.front ? 1 / m.ior : m.ior;
      if (!refract(ray.d, hit.n, eta, dir) || rng.next_float() < schlick(-dot(ray.d, hit.n), eta))
        dir = reflect(ray.d, hit.n);
    }
    throughput = throughput * albedo;
    if (b >= 3) {
      float p = fminf(fmaxf(fmaxf(throughput.x, fmaxf(throughput.y, throughput.z)), 0.05f), 0.95f);
      if (rng.next_float() > p) break;
      throughput = throughput / p;
    }
    ray = {hit.p + hit.n * (dot(dir, hit.n) > 0 ? T_MIN : -T_MIN), dir};
  }
  add(r.normal_depth[i], aov_n, aov_d);
  add(r.albedo[i], aov_a);
  float l = luminance(color);
  if (isfinite(color.x) && isfinite(color.y) && isfinite(color.z)) add(r.accum[i], color, l * l);
}

__device__ uint8_t unit_byte(float x) { return uint8_t(fminf(fmaxf(x, 0.0f), 1.0f) * 255 + 0.5f); }

// .w = std of the mean's luminance; below 4 spp the estimate is useless, so huge std disables the color term
__global__ void mean_kernel(const float4* accum, float4* out, int w, int h, int spp) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  float inv = 1.0f / spp;
  float4 a = accum[y * w + x];
  float l = luminance({a.x * inv, a.y * inv, a.z * inv});
  float std = spp < 4 ? 1e20f : sqrtf(fmaxf(a.w * inv - l * l, 0.0f) * inv);
  out[y * w + x] = make_float4(a.x * inv, a.y * inv, a.z * inv, std);
}

__global__ void resolve_kernel(Renderer r, Settings st, const float4* color, const float4* glow, uchar4* out) {
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
    float4 c = color[i];
    if (glow) c = make_float4(c.x + st.bloom_strength * glow[i].x, c.y + st.bloom_strength * glow[i].y, c.z + st.bloom_strength * glow[i].z, 0);
    float e = st.exposure;
    out[i] = make_uchar4(encode(c.x, e, st.tonemap_aces), encode(c.y, e, st.tonemap_aces), encode(c.z, e, st.tonemap_aces), 255);
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
  r->tris = upload(scene.tris);
  r->nodes = upload(scene.bvh);
  r->env = scene.env.empty() ? nullptr : upload(scene.env);
  std::vector<float> marginal, conditional;
  if (r->env) build_env_cdf(scene.env.data(), scene.env_w, scene.env_h, marginal, conditional);
  r->env_marginal = marginal.empty() ? nullptr : upload(marginal);
  r->env_conditional = marginal.empty() ? nullptr : upload(conditional);
  r->view = {r->spheres, int(scene.spheres.size()), r->planes, int(scene.planes.size()), r->mats, scene.sky,
             r->tris, int(scene.tris.size()), r->nodes, r->env, scene.env_w, scene.env_h, r->env_marginal, r->env_conditional};
  r->w = w;
  r->h = h;
  size_t bytes = size_t(w) * h * sizeof(float4);
  cudaMalloc(&r->accum, bytes);
  cudaMalloc(&r->normal_depth, bytes);
  cudaMalloc(&r->albedo, bytes);
  for (float4** p : {&r->ping, &r->pong, &r->glow, &r->glow_tmp}) cudaMalloc(p, bytes);
  reset_accumulation(r);
  return r;
}

void destroy_renderer(Renderer* r) {
  for (void* p : {(void*)r->spheres, (void*)r->planes, (void*)r->mats, (void*)r->tris, (void*)r->nodes, (void*)r->env, (void*)r->env_marginal, (void*)r->env_conditional,
                  (void*)r->accum, (void*)r->normal_depth, (void*)r->albedo, (void*)r->ping, (void*)r->pong, (void*)r->glow,
                  (void*)r->glow_tmp})
    cudaFree(p);
  delete r;
}

void reset_accumulation(Renderer* r) {
  size_t bytes = size_t(r->w) * r->h * sizeof(float4);
  cudaMemset(r->accum, 0, bytes);
  cudaMemset(r->normal_depth, 0, bytes);
  cudaMemset(r->albedo, 0, bytes);
  r->spp = 0;
}

void accumulate(Renderer* r, const Camera& cam, const Settings& settings, int spp) {
  for (int f = 0; f < spp; f++) path_kernel<<<grid_for(r), dim3(16, 16)>>>(*r, cam, settings, r->spp + f);
  r->spp += spp;
}

void resolve(Renderer* r, const Settings& st, void* device_rgba) {
  float* color = nullptr;
  float4* glow = nullptr;
  if (st.view == VIEW_COLOR) {
    mean_kernel<<<grid_for(r), dim3(16, 16)>>>(r->accum, r->ping, r->w, r->h, r->spp);
    color = (float*)r->ping;
    if (st.denoise) color = denoise(color, (float*)r->pong, (float*)r->normal_depth, (float*)r->albedo, r->w, r->h, r->spp, st);
    if (st.bloom) {
      bloom(color, (float*)r->glow, (float*)r->glow_tmp, r->w, r->h, st);
      glow = r->glow;
    }
  }
  resolve_kernel<<<grid_for(r), dim3(16, 16)>>>(*r, st, (float4*)color, glow, (uchar4*)device_rgba);
}

int accumulated_spp(const Renderer* r) { return r->spp; }

std::vector<uint8_t> render_scene(const Scene& scene, const Settings& settings, int w, int h, int spp) {
  Renderer* r = create_renderer(scene, w, h);
  accumulate(r, scene.cam, settings, spp);
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
  accumulate(r, scene.cam, Settings{}, spp);
  std::vector<float4> sums(size_t(w) * h);
  cudaMemcpy(sums.data(), r->accum, sums.size() * sizeof(float4), cudaMemcpyDeviceToHost);
  destroy_renderer(r);
  std::vector<float> rgb;
  rgb.reserve(sums.size() * 3);
  for (float4 a : sums) rgb.insert(rgb.end(), {a.x / spp, a.y / spp, a.z / spp});
  return rgb;
}
