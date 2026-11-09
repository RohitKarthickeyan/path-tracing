#include <cuda_runtime.h>
#include "render.h"

__global__ void gradient_kernel(uchar4* out, int w, int h) {
  int x = blockIdx.x * blockDim.x + threadIdx.x;
  int y = blockIdx.y * blockDim.y + threadIdx.y;
  if (x >= w || y >= h) return;
  float r = float(x) / (w - 1), g = 1.0f - float(y) / (h - 1);
  out[y * w + x] = make_uchar4(uint8_t(r * 255 + 0.5f), uint8_t(g * 255 + 0.5f), 64, 255);
}

std::vector<uint8_t> render_gradient(int w, int h) {
  uchar4* d = nullptr;
  cudaMalloc(&d, size_t(w) * h * sizeof(uchar4));
  dim3 block(16, 16), grid((w + 15) / 16, (h + 15) / 16);
  gradient_kernel<<<grid, block>>>(d, w, h);
  std::vector<uint8_t> px(size_t(w) * h * 4);
  cudaMemcpy(px.data(), d, px.size(), cudaMemcpyDeviceToHost);
  cudaFree(d);
  return px;
}
