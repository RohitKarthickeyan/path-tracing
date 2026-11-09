#include <algorithm>
#include "bvh.h"

static Vec3 centroid(const Triangle& t) { return (t.v0 + t.v1 + t.v2) / 3; }

// ponytail: median split on the longest centroid axis; binned SAH if traversal is too slow
static void build(std::vector<Triangle>& tris, std::vector<BvhNode>& nodes, int ni, int first, int count) {
  Aabb box{tris[first].v0, tris[first].v0}, cbox{centroid(tris[first]), centroid(tris[first])};
  for (int i = first; i < first + count; i++) {
    const Triangle& t = tris[i];
    box = {vmin(vmin(box.lo, t.v0), vmin(t.v1, t.v2)), vmax(vmax(box.hi, t.v0), vmax(t.v1, t.v2))};
    cbox = {vmin(cbox.lo, centroid(t)), vmax(cbox.hi, centroid(t))};
  }
  nodes[ni].box = box;
  if (count <= 4) {
    nodes[ni].left_or_first = first;
    nodes[ni].count = count;
    return;
  }
  Vec3 e = cbox.hi - cbox.lo;
  int axis = e.x > e.y && e.x > e.z ? 0 : e.y > e.z ? 1 : 2;
  auto key = [axis](const Triangle& t) {
    Vec3 c = centroid(t);
    return axis == 0 ? c.x : axis == 1 ? c.y : c.z;
  };
  int mid = first + count / 2;
  std::nth_element(tris.begin() + first, tris.begin() + mid, tris.begin() + first + count,
                   [&](const Triangle& a, const Triangle& b) { return key(a) < key(b); });
  int left = int(nodes.size());
  nodes.resize(nodes.size() + 2);
  nodes[ni].left_or_first = left;
  nodes[ni].count = 0;
  build(tris, nodes, left, first, mid - first);
  build(tris, nodes, left + 1, mid, first + count - mid);
}

void build_bvh(std::vector<Triangle>& tris, std::vector<BvhNode>& nodes) {
  nodes.clear();
  if (tris.empty()) return;
  nodes.resize(1);
  build(tris, nodes, 0, 0, int(tris.size()));
}
