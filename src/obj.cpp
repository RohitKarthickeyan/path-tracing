#include <fstream>
#include <sstream>
#include <string>
#include "obj.h"

struct Corner { int v, n; };

static int resolve(int i, size_t count) { return i < 0 ? int(count) + i : i - 1; }

bool load_obj(const char* path, int mat, std::vector<Triangle>& out) {
  std::ifstream f(path);
  if (!f) return false;
  std::vector<Vec3> v, vn;
  std::vector<Corner> corners;
  std::string line, tag, tok;
  try {
    while (std::getline(f, line)) {
      std::istringstream ss(line);
      ss >> tag;
      if (tag == "v" || tag == "vn") {
        Vec3 p;
        ss >> p.x >> p.y >> p.z;
        (tag == "v" ? v : vn).push_back(p);
      } else if (tag == "f") {
        std::vector<Corner> poly;
        while (ss >> tok) {
          size_t s2 = tok.find('/', tok.find('/') + 1);
          int n = s2 != std::string::npos && s2 + 1 < tok.size() ? resolve(std::stoi(tok.substr(s2 + 1)), vn.size()) : -1;
          poly.push_back({resolve(std::stoi(tok), v.size()), n});
        }
        for (size_t k = 1; k + 1 < poly.size(); k++) corners.insert(corners.end(), {poly[0], poly[k], poly[k + 1]});
      }
    }
  } catch (const std::exception&) {
    return false;
  }
  for (const Corner& c : corners)
    if (c.v < 0 || c.v >= int(v.size()) || c.n >= int(vn.size())) return false;
  if (corners.empty()) return false;

  std::vector<Vec3> smooth(v.size(), {0, 0, 0});
  for (size_t i = 0; i < corners.size(); i += 3) {
    Vec3 a = v[corners[i].v], b = v[corners[i + 1].v], c = v[corners[i + 2].v];
    Vec3 face = cross(b - a, c - a);
    for (int k = 0; k < 3; k++) smooth[corners[i + k].v] = smooth[corners[i + k].v] + face;
  }
  auto normal = [&](Corner c) { return c.n >= 0 ? normalize(vn[c.n]) : normalize(smooth[c.v]); };
  out.clear();
  for (size_t i = 0; i < corners.size(); i += 3) {
    Corner a = corners[i], b = corners[i + 1], c = corners[i + 2];
    out.push_back({v[a.v], v[b.v], v[c.v], normal(a), normal(b), normal(c), mat});
  }
  return true;
}
