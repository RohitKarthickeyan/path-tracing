#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <GLFW/glfw3.h>
#include <cuda_gl_interop.h>
#include "image_io.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include "render.h"
#include "scenes.h"
#include "viewer.h"

extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 1;

constexpr GLenum PIXEL_UNPACK_BUFFER = 0x88EC, STREAM_DRAW = 0x88E0;
using GenBuffers = void(__stdcall*)(GLsizei, GLuint*);
using BindBuffer = void(__stdcall*)(GLenum, GLuint);
using BufferData = void(__stdcall*)(GLenum, ptrdiff_t, const void*, GLenum);

static const char* SCENES[] = {"sphere", "furnace", "cornell", "spheres", "bunny"};

static bool fly(GLFWwindow* win, Camera& cam, float dt, double& last_x, double& last_y, bool enabled) {
  bool moved = false;
  double x, y;
  glfwGetCursorPos(win, &x, &y);
  if (enabled && glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && (x != last_x || y != last_y)) {
    cam.yaw += float(x - last_x) * 0.003f;
    cam.pitch = fminf(fmaxf(cam.pitch - float(y - last_y) * 0.003f, -1.5f), 1.5f);
    moved = true;
  }
  last_x = x;
  last_y = y;
  if (!enabled) return false;
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

static void on_key(GLFWwindow* win, int key, int, int action, int) {
  if (action != GLFW_PRESS || ImGui::GetIO().WantCaptureKeyboard) return;
  Settings& st = *(Settings*)glfwGetWindowUserPointer(win);
  if (key >= GLFW_KEY_1 && key <= GLFW_KEY_4) st.view = ViewMode(key - GLFW_KEY_1);
  if (key == GLFW_KEY_N) st.denoise = !st.denoise;
  if (key == GLFW_KEY_B) st.bloom = !st.bloom;
  if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// hdri: 0 = scene default, 1 = none, 2+ = hdrs[hdri - 2]
static Scene load_scene(const std::string& name, int hdri, const std::vector<std::string>& hdrs) {
  Scene s;
  make_scene(name, s);
  if (hdri == 1) s.env.clear();
  if (hdri >= 2) load_hdr(hdrs[hdri - 2].c_str(), s.env_w, s.env_h, s.env);
  return s;
}

// Returns true when a change invalidates the accumulated samples.
static bool panel(Settings& st, Camera& cam, int& scene_i, int& hdri_i, const std::vector<const char*>& hdri_names, bool& reload,
                  float fps, int spp) {
  bool reset = false;
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
  ImGui::Begin("pt", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%.0f fps | %d spp", fps, spp);
  reload |= ImGui::Combo("scene", &scene_i, SCENES, IM_ARRAYSIZE(SCENES));
  reload |= ImGui::Combo("hdri", &hdri_i, hdri_names.data(), int(hdri_names.size()));
  ImGui::SeparatorText("sampling");
  reset |= ImGui::SliderInt("spp / frame", &st.spp_per_frame, 1, 16);
  reset |= ImGui::SliderInt("max bounces", &st.max_bounces, 1, 32);
  reset |= ImGui::SliderFloat("env intensity", &st.env_intensity, 0, 4);
  reset |= ImGui::SliderAngle("env rotation", &st.env_rotation, -180, 180);
  ImGui::SeparatorText("camera");
  reset |= ImGui::SliderFloat("fov", &cam.vfov, 10, 120);
  reset |= ImGui::SliderFloat("aperture", &cam.aperture, 0, 0.5f);
  reset |= ImGui::SliderFloat("focus dist", &cam.focus_dist, 0.1f, 30);
  ImGui::SeparatorText("display");
  ImGui::SliderFloat("exposure", &st.exposure, 0.05f, 8, "%.2f", ImGuiSliderFlags_Logarithmic);
  ImGui::Checkbox("ACES", &st.tonemap_aces);
  int view = st.view;
  ImGui::Combo("view", &view, "color\0normal\0albedo\0depth\0");
  st.view = ViewMode(view);
  ImGui::SeparatorText("denoiser");
  ImGui::Checkbox("denoise", &st.denoise);
  ImGui::SliderInt("iterations", &st.denoise_iters, 1, 5);
  ImGui::SliderFloat("sigma normal", &st.sigma_n, 1, 256, "%.0f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("sigma albedo", &st.sigma_a, 0.01f, 1, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("sigma depth", &st.sigma_z, 0.01f, 1, "%.3f", ImGuiSliderFlags_Logarithmic);
  ImGui::SliderFloat("sigma color", &st.sigma_c, 0.1f, 10, "%.2f", ImGuiSliderFlags_Logarithmic);
  ImGui::SeparatorText("bloom");
  ImGui::Checkbox("bloom", &st.bloom);
  ImGui::SliderFloat("threshold", &st.bloom_threshold, 0, 10);
  ImGui::SliderFloat("knee", &st.bloom_knee, 0.01f, 2);
  ImGui::SliderFloat("strength", &st.bloom_strength, 0, 0.5f);
  ImGui::SliderInt("radius", &st.bloom_radius, 1, 64);
  ImGui::End();
  return reset;
}

void run_viewer(const std::string& scene_name, Settings settings, int w, int h) {
  glfwInit();
  GLFWwindow* win = glfwCreateWindow(w, h, "pt", nullptr, nullptr);
  glfwMakeContextCurrent(win);
  glfwSwapInterval(0);
  glfwSetWindowUserPointer(win, &settings);
  glfwSetKeyCallback(win, on_key);
  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(win, true);
  ImGui_ImplOpenGL2_Init();

  auto gen_buffers = (GenBuffers)glfwGetProcAddress("glGenBuffers");
  auto bind_buffer = (BindBuffer)glfwGetProcAddress("glBindBuffer");
  auto buffer_data = (BufferData)glfwGetProcAddress("glBufferData");
  GLuint pbo;
  gen_buffers(1, &pbo);
  cudaGraphicsResource* res = nullptr;
  auto resize_pbo = [&]() {
    if (res) cudaGraphicsUnregisterResource(res);
    bind_buffer(PIXEL_UNPACK_BUFFER, pbo);
    buffer_data(PIXEL_UNPACK_BUFFER, ptrdiff_t(w) * h * 4, nullptr, STREAM_DRAW);
    bind_buffer(PIXEL_UNPACK_BUFFER, 0);
    cudaGraphicsGLRegisterBuffer(&res, pbo, cudaGraphicsMapFlagsWriteDiscard);
  };
  glfwGetFramebufferSize(win, &w, &h);
  resize_pbo();
  ImGui::GetIO().FontGlobalScale = h / 720.0f;  // panel sized for 720 px

  std::vector<std::string> hdrs;
  for (const auto& e : std::filesystem::directory_iterator("assets"))
    if (e.path().extension() == ".hdr") hdrs.push_back(e.path().generic_string());
  std::vector<const char*> hdri_names = {"scene default", "none"};
  for (const std::string& p : hdrs) hdri_names.push_back(p.c_str());
  int scene_i = 0, hdri_i = 0;
  for (int i = 0; i < IM_ARRAYSIZE(SCENES); i++)
    if (scene_name == SCENES[i]) scene_i = i;

  Scene scene = load_scene(SCENES[scene_i], hdri_i, hdrs);
  Renderer* r = create_renderer(scene, w, h);
  Camera cam = scene.cam;
  double last_x = 0, last_y = 0, last_t = glfwGetTime(), title_t = last_t;
  glfwGetCursorPos(win, &last_x, &last_y);
  int frames = 0;
  float fps = 0;
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    bool reload = false;
    bool reset = panel(settings, cam, scene_i, hdri_i, hdri_names, reload, fps, accumulated_spp(r));
    if (reload) {
      destroy_renderer(r);
      scene = load_scene(SCENES[scene_i], hdri_i, hdrs);
      r = create_renderer(scene, w, h);
      cam = scene.cam;
    }
    int fw, fh;
    glfwGetFramebufferSize(win, &fw, &fh);
    if (fw > 0 && fh > 0 && (fw != w || fh != h)) {
      w = fw;
      h = fh;
      resize_pbo();
      destroy_renderer(r);
      r = create_renderer(scene, w, h);
      glViewport(0, 0, w, h);
      ImGui::GetIO().FontGlobalScale = h / 720.0f;
    }
    double t = glfwGetTime();
    ImGuiIO& io = ImGui::GetIO();
    if (fly(win, cam, float(t - last_t), last_x, last_y, !io.WantCaptureMouse && !io.WantCaptureKeyboard)) reset = true;
    if (reset && !reload) reset_accumulation(r);
    last_t = t;

    accumulate(r, cam, settings, settings.spp_per_frame);
    void* px;
    size_t size;
    cudaGraphicsMapResources(1, &res);
    cudaGraphicsResourceGetMappedPointer(&px, &size, res);
    resolve(r, settings, px);
    cudaGraphicsUnmapResources(1, &res);
    bind_buffer(PIXEL_UNPACK_BUFFER, pbo);
    glPixelZoom(1, -1);
    glRasterPos2f(-1, 1);
    glDrawPixels(w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    bind_buffer(PIXEL_UNPACK_BUFFER, 0);
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(win);

    frames++;
    if (t - title_t > 0.5) {
      fps = float(frames / (t - title_t));
      char title[64];
      std::snprintf(title, sizeof(title), "pt | %.0f fps | %d spp", fps, accumulated_spp(r));
      glfwSetWindowTitle(win, title);
      frames = 0;
      title_t = t;
    }
  }
  destroy_renderer(r);
  cudaGraphicsUnregisterResource(res);
  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwTerminate();
}
