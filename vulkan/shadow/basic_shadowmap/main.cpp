#include <exception>
#include <stdexcept>

#include "config.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "ShadowView.h"
#include "VulkanInstance.h"

int main(int argc, char **argv)
{
  SDL_Window *win = 0;
  VkSurfaceKHR surface = 0;
  std::shared_ptr<ShadowView> view = 0;
  try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
      throw std::runtime_error("sdl init error.");
    win = SDL_CreateWindow("demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (win == nullptr)
      throw std::runtime_error("could not create sdl window.");

    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);

    auto &inst = VulkanInstance::instance();
    if (!SDL_Vulkan_CreateSurface(win, inst, &surface))
      throw std::runtime_error("could not create vk surface.");

    inst.enable_debug();
    auto dev = inst.create_device();

    view = std::make_shared<ShadowView>(dev);
    view->set_surface(surface, w, h);
    view->create_pipeline();
  } catch (std::runtime_error &e) {
    printf("%s", e.what());
    return -1;
  }
  view->frame();
  return 0;
}