#pragma once
#include <cmath>
#include <cstdint>

#ifdef __CUDACC__
#define HD __host__ __device__
#else
#define HD
#endif

constexpr float PI = 3.14159265f;

struct Vec3 { float x, y, z; };

HD inline Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
HD inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
HD inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
HD inline Vec3 operator*(Vec3 a, Vec3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
HD inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
HD inline Vec3 operator*(float s, Vec3 a) { return a * s; }
HD inline Vec3 operator/(Vec3 a, float s) { return a * (1.0f / s); }
HD inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
HD inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
HD inline float length(Vec3 a) { return sqrtf(dot(a, a)); }
HD inline Vec3 normalize(Vec3 a) { return a / length(a); }

struct Ray {
  Vec3 o, d;
  HD Vec3 at(float t) const { return o + t * d; }
};

// PCG hash, Jarzynski & Olano 2020
HD inline uint32_t pcg_hash(uint32_t v) {
  uint32_t state = v * 747796405u + 2891336453u;
  uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}

struct Rng {
  uint32_t state;
  HD float next_float() {
    state = pcg_hash(state);
    return (state >> 8) * (1.0f / 16777216.0f);
  }
};

HD inline Rng rng_seed(uint32_t pixel, uint32_t frame) { return {pcg_hash(pixel ^ pcg_hash(frame))}; }
