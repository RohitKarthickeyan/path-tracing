#pragma once
#include <vector>
#include "scene.h"

bool load_obj(const char* path, int mat, std::vector<Triangle>& out);
