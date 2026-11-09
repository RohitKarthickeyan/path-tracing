#include <cstddef>
#include <cstdio>
#include <GLFW/glfw3.h>
#include <cuda_gl_interop.h>
#include "render.h"
#include "viewer.h"

extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 1;

constexpr GLenum PIXEL_UNPACK_BUFFER = 0x88EC, STREAM_DRAW = 0x88E0;
using GenBuffers = void(__stdcall*)(GLsizei, GLuint*);
using BindBuffer = void(__stdcall*)(GLenum, GLuint);
using BufferData = void(__stdcall*)(GLenum, ptrdiff_t, const void*, GLenum);

static bool fly(GLFWwindow* win, Camera& cam, float dt, double& last_x, double& last_y) {
  bool moved = false;
  double x, y;
  glfwGetCursorPos(win, &x, &y);
  if (glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && (x != last_x || y != last_y)) {
    cam.yaw += float(x - last_x) * 0.003f;
    cam.pitch = fminf(fmaxf(cam.pitch - float(y - last_y) * 0.003f, -1.5f), 1.5f);
    moved = true;
  }
  last_x = x;
  last_y = y;
  Vec3 fwd = camera_forward(cam), right = normalize(cross(fwd, Vec3{0, 1, 0})), up{0, 1, 0};
  float speed = dt * (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 6.0f : 1.5f);
  struct { int key; Vec3 dir; } keys[] = {
      {GLFW_KEY_W, fwd}, {GLFW_KEY_S, -fwd}, {GLFW_KEY_D, right}, {GLFW_KEY_A, -right}, {GLFW_KEY_E, up}, {GLFW_KEY_Q, -up}};
  for (auto& k : keys) {
    if (glfwGetKey(win, k.key) != GLFW_PRESS) continue;
    cam.pos = cam.pos + speed * k.dir;
    moved = true;
  }
  return moved;
}

void run_viewer(const Scene& scene, Settings settings, int w, int h) {
  glfwInit();
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  GLFWwindow* win = glfwCreateWindow(w, h, "pt", nullptr, nullptr);
  glfwMakeContextCurrent(win);
  glfwSwapInterval(0);
  auto gen_buffers = (GenBuffers)glfwGetProcAddress("glGenBuffers");
  auto bind_buffer = (BindBuffer)glfwGetProcAddress("glBindBuffer");
  auto buffer_data = (BufferData)glfwGetProcAddress("glBufferData");

  GLuint pbo;
  gen_buffers(1, &pbo);
  bind_buffer(PIXEL_UNPACK_BUFFER, pbo);
  buffer_data(PIXEL_UNPACK_BUFFER, ptrdiff_t(w) * h * 4, nullptr, STREAM_DRAW);
  cudaGraphicsResource* res;
  cudaGraphicsGLRegisterBuffer(&res, pbo, cudaGraphicsMapFlagsWriteDiscard);
  glPixelZoom(1, -1);
  glRasterPos2f(-1, 1);

  Renderer* r = create_renderer(scene, w, h);
  Camera cam = scene.cam;
  double last_x = 0, last_y = 0, last_t = glfwGetTime(), title_t = last_t;
  glfwGetCursorPos(win, &last_x, &last_y);
  int frames = 0;
  const int view_keys[] = {GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4};
  while (!glfwWindowShouldClose(win) && glfwGetKey(win, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
    glfwPollEvents();
    double t = glfwGetTime();
    if (fly(win, cam, float(t - last_t), last_x, last_y)) reset_accumulation(r);
    last_t = t;
    for (int v = 0; v < 4; v++)
      if (glfwGetKey(win, view_keys[v]) == GLFW_PRESS) settings.view = ViewMode(v);

    accumulate(r, cam, settings, 1);
    void* px;
    size_t size;
    cudaGraphicsMapResources(1, &res);
    cudaGraphicsResourceGetMappedPointer(&px, &size, res);
    resolve(r, settings, px);
    cudaGraphicsUnmapResources(1, &res);
    glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glfwSwapBuffers(win);

    frames++;
    if (t - title_t > 0.5) {
      char title[64];
      std::snprintf(title, sizeof(title), "pt | %.0f fps | %d spp", frames / (t - title_t), accumulated_spp(r));
      glfwSetWindowTitle(win, title);
      frames = 0;
      title_t = t;
    }
  }
  destroy_renderer(r);
  cudaGraphicsUnregisterResource(res);
  glfwTerminate();
}
