#include <exception>
#include <stdexcept>

#include "Manipulator.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "VulkanDebug.h"
#include "VulkanView.h"
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"
#include "VulkanTexture.h"

#include "SimpleShape.h"

#include "VulkanView.h"
#include "GLTFLoader.h"
#include "MeshInstance.h"
#include "config.h"

#define SHADER_DIR ROOT_DIR##"/vulkan/tbdr"

#define WM_PAINT 1

constexpr float fov = 60;

VulkanInstance &inst = VulkanInstance::instance();


struct {
  struct alignas(16) aligned_vec3 : vec3 {};
  aligned_vec3 light_pos[4] = {vec3(10, -10, 10), vec3(-10, -10, 10), vec3(-10, -10, -10), vec3(10, -10, -10)};
  aligned_vec3 light_color[4] = {vec3(300), vec3(300), vec3(300), vec3(300)};
} lights_ubo;

struct alignas(16) {
  struct alignas(16) aligned_vec3 : vec3 {};

  float metallic;
  float roughness;
  float ao;
  aligned_vec3 albedo;
} material_ubo;

class Test : public VulkanView {
public:
  Test(const std::shared_ptr<VulkanDevice> &dev) : VulkanView(dev, false) { 

    GLTFLoader loader(_device);
    //_mesh = loader.load_file(ROOT_DIR "/data/deer.gltf");
    //_mesh = loader.load_file(ROOT_DIR "/data/vulkanscenemodels.gltf");
    _mesh = loader.load_file(ROOT_DIR "/data/oaktree.gltf");
    //_mesh = loader.load_file("D:\\01_work\\hcmodel\\garbage\\grabage.gltf");
  }

  ~Test() 
  {
    vkDeviceWaitIdle(*_device);
  }

  void set_window(SDL_Window *win)
  {
    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(win, inst, &surface))
      throw std::runtime_error("could not create vk surface.");

    set_surface(surface, w, h);

    if(_mesh)
      _mesh->create_pipeline(render_pass());

    build_command_buffers();

    update_frame();
  }

  void wheel(int delta) { update_ubo(); }
  void left_drag(int x, int y, int, int) { update_ubo(); }
  void right_drag(int x, int y, int, int) { update_ubo(); }

  void resize(int, int) { update_ubo(); }
  void build_command_buffer(VkCommandBuffer cmd_buf) 
  { 
    if(_mesh) _mesh->build_command_buffer(cmd_buf);
  }

  void update_ubo()
  {
    auto prj = tg::perspective<float>(fov, float(width()) / height(), 0.1, 1000);
    auto &manip = manipulator();
    if(_mesh) _mesh->set_vp(prj, manip.view_matrix(), manip.eye());
  }

private:
  VkPipelineLayout _pipe_layout = VK_NULL_HANDLE;

  VkPipeline _pipeline = VK_NULL_HANDLE;

  std::shared_ptr<MeshInstance> _mesh;
};

int main(int argc, char **argv)
{
  SDL_Window *win = 0;
  std::shared_ptr<Test> test;
  try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
      throw std::runtime_error("sdl init error.");
    win = SDL_CreateWindow("demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (win == nullptr)
      throw std::runtime_error("could not create sdl window.");

    inst.enable_debug();
    auto dev = inst.create_device();

    test = std::make_shared<Test>(dev);
    test->set_window(win);

  } catch (std::runtime_error &e) {
    printf("%s", e.what());
    return -1;
  }
  test->frame();
  return 0;
}