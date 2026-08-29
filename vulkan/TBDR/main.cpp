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
  aligned_vec3 lightPos[4] = {vec3(10, -10, 10), vec3(-10, -10, 10), vec3(-10, -10, -10), vec3(10, -10, -10)};
  aligned_vec3 lightColor[4] = {vec3(300), vec3(300), vec3(300), vec3(300)};
} lightsUbo;

struct alignas(16) {
  struct alignas(16) aligned_vec3 : vec3 {};

  float metallic;
  float roughness;
  float ao;
  aligned_vec3 albedo;
} materialUbo;

class Test : public VulkanView {
public:
  Test(const std::shared_ptr<VulkanDevice> &dev) : VulkanView(dev, false) {

    GLTFLoader loader(_device);
    //_mesh = loader.loadFile(ROOT_DIR "/data/deer.gltf");
    //_mesh = loader.loadFile(ROOT_DIR "/data/vulkanscenemodels.gltf");
    _mesh = loader.loadFile(ROOT_DIR "/data/oaktree.gltf");
    //_mesh = loader.loadFile("D:\\01_work\\hcmodel\\garbage\\grabage.gltf");
  }

  ~Test()
  {
    vkDeviceWaitIdle(*_device);
  }

  void setWindow(SDL_Window *win)
  {
    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(win, inst, &surface))
      throw std::runtime_error("could not create vk surface.");

    setSurface(surface, w, h);

    if(_mesh)
      _mesh->createPipeline(renderPass());

    createCommandBuffers();

    updateFrame();
  }

  void wheel(int delta) { updateUbo(); }
  void leftDrag(int x, int y, int, int) { updateUbo(); }
  void rightDrag(int x, int y, int, int) { updateUbo(); }

  void resize(int, int) { updateUbo(); }
  void buildCommandBuffer(VkCommandBuffer cmdBuf)
  {
    if(_mesh) _mesh->buildCommandBuffer(cmdBuf);
  }

  void updateUbo()
  {
    auto prj = tg::perspective<float>(fov, float(width()) / height(), 0.1, 1000);
    auto &manip = manipulator();
    if(_mesh) _mesh->set_vp(prj, manip->viewMatrix(), manip->eye());
  }

private:
  VkPipelineLayout _pipeLayout = VK_NULL_HANDLE;

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

    inst.enableDebug();
    auto dev = inst.createDevice();

    test = std::make_shared<Test>(dev);
    test->setWindow(win);

  } catch (std::runtime_error &e) {
    printf("%s", e.what());
    return -1;
  }
  test->frame();
  return 0;
}
