#pragma once
#include <cstdint>
#include <vector>
#include "scene.h"

std::vector<uint8_t> render_scene(const Scene& scene, int w, int h, int spp);
