# Architecture

## Pipeline

Every displayed frame runs this chain on the GPU. Color stays in linear floating point until the final stage.

```
scene (spheres, planes, BVH triangles, materials, HDRI + its CDFs)
  │
  ▼
path_kernel ×N   one thread per pixel, one sample per launch
  │              accum += color (and luminance² in .w), AOVs += normal/depth, albedo
  ▼
mean_kernel      accum / spp, plus each pixel's noise std in .w
  ▼
denoise          5 à-trous passes, taps spaced 1, 2, 4, 8, 16 px (ping-pong buffers)
  ▼
bloom            bright pass → horizontal Gaussian → vertical Gaussian
  ▼
resolve_kernel   + bloom × strength → exposure → ACES → gamma 2.2 → RGBA8
  ▼
OpenGL pixel buffer (viewer, mapped into CUDA, no CPU copy)  or  PNG (CLI)
```

Accumulation restarts whenever the camera, the scene or a sampling setting changes. The denoiser, bloom and tonemapping run on the accumulated mean every frame, so toggling them is instant.

## Source layout (`src/`)

| layer | files | role |
|---|---|---|
| shared math (host + device) | `hd.h`, `camera.h`, `scene.h`, `intersect.h`, `sampling.h`, `filters.h` | Vec3, RNG, rays, camera, materials, intersection, BVH traversal, sampling, filter weights. Header-only and `__host__ __device__`, so the CPU unit tests run them without a GPU. |
| host | `image_io.cpp`, `obj.cpp`, `bvh.cpp`, `scenes.cpp` | PNG/HDR I/O, the OBJ loader, BVH build, scene definitions |
| device | `render.cu`, `filters.cu` | the path-trace kernel, the Renderer's buffers, and the filter kernels |
| app | `viewer.cpp`, `main.cpp` | GLFW window, CUDA-GL interop, ImGui, fly camera, and the CLI |

A grep check (`scripts/check_includes.sh`) enforces these layer boundaries.

## Key algorithms

**Bounce loop** (`render.cu`). The loop is iterative, because GPU threads have small stacks. It carries a throughput (the running color tint) and adds emission or sky light when a path ends. Ray origins are offset along the normal, on the side the ray leaves from, to avoid self-intersection. Samples that come out as NaN or infinity are dropped.

**Materials.**
- Diffuse: cosine-weighted bounce, built as the normal plus a uniform unit-sphere point.
- Metal: reflection plus a random fuzz offset.
- Glass: Snell refraction, or reflection on total internal reflection, chosen randomly by Schlick's Fresnel term.
- Emissive: ends the path.

**Russian roulette.** From the third bounce on, a path survives with probability p = max(throughput), clamped to [0.05, 0.95]. Survivors are divided by p, so the estimate stays unbiased.

**HDRI importance sampling + MIS** (`sampling.h`).
- Each pixel of the environment map is weighted by luminance × sin θ. A marginal CDF over rows and a conditional CDF per row let `env_sample` pick a pixel, jitter within it, and return a direction with its probability density, which is the pixel's probability × w·h / (2π² sin θ).
- At every diffuse hit, a shadow ray goes toward one sampled sky direction. Its contribution is weighted with the balance heuristic against the cosine probability.
- A bounce ray that escapes to the sky after a diffuse hit gets the complementary weight.

**BVH** (`bvh.cpp`, `intersect.h`).
- The CPU builds it recursively, splitting at the median along the longest axis of the triangle centroids, with at most 4 triangles per leaf.
- It's stored as a flat array of nodes, with a node's two children next to each other.
- The GPU walks it with a 64-entry stack, and a slab test against each node's box decides whether to visit it.
- Triangles use the Möller–Trumbore test. The flat face normal decides inside versus outside; interpolated vertex normals handle shading.

**À-trous denoiser** (`filters.cu`).
- A 5×5 kernel h = [1/16, 1/4, 3/8, 1/4, 1/16] ⊗ h, applied 5 times with tap spacing 2ⁱ.
- Each tap's weight is multiplied by edge-stopping terms:
  - normal: `max(0, n·n_q)^σn`
  - albedo: `exp(−|Δa|²/σa²)`
  - relative depth: `exp(−|Δz| / (σz·z))`
  - color: `exp(−|Δl| / (σc·std))`
- `std` is the pixel's noise level, the standard deviation of its mean luminance, computed from accumulated L and L². Real edges sit many standard deviations apart and survive; noise is about one apart and gets averaged out. The color term is off below 4 spp, where the noise estimate is unreliable. Pixels with no geometry (sky) pass through unchanged.

**Bloom.**
- A 3×3 bright pass averages pixels weighted by 1/(1 + luminance), so a lone firefly barely contributes.
- A soft-knee threshold follows, then a separable Gaussian (two 1D passes, radius r, σ = r/3).
- The result is added before tonemapping.

## Validation

- **Furnace:** a diffuse sphere (albedo 0.8) in a uniform white sky must give 0.8 exactly in linear color (230 after gamma), both under a constant sky and under a uniform HDRI. The HDRI case checks that the MIS weights add up correctly.
- **Russian roulette:** must not change the Cornell box's mean brightness.
- **BVH:** must return the same hits as brute force on 5000 rays.
- **Unit tests:** cover intersection, the sampling distributions, filter weights and the OBJ parser.
