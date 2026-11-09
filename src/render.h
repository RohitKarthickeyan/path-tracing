#pragma once
#include <cstdint>
#include <vector>
#include "scene.h"

struct Renderer;
Renderer* create_renderer(const Scene& scene, int w, int h);
void destroy_renderer(Renderer* r);
void reset_accumulation(Renderer* r);
void accumulate(Renderer* r, const Camera& cam, const Settings& settings, int spp);
void resolve(Renderer* r, const Settings& settings, void* device_rgba);
int accumulated_spp(const Renderer* r);

std::vector<uint8_t> render_scene(const Scene& scene, const Settings& settings, int w, int h, int spp);
std::vector<float> render_linear(const Scene& scene, int w, int h, int spp);  // mean RGB per pixel
