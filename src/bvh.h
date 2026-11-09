#pragma once
#include <vector>
#include "scene.h"

void build_bvh(std::vector<Triangle>& tris, std::vector<BvhNode>& nodes);  // reorders tris
