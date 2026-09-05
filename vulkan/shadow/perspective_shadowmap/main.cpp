#include <exception>
#include <stdexcept>

#include "config.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include "ShadowView.h"
#include "VulkanInstance.h"

int main(int argc, char **argv)
{
  SDL_Window *win = 0;
  VkSurfaceKHR surface = 0;
  std::shared_ptr<ShadowView> view = 0;
  try {
    if (!SDL_Init(SDL_INIT_VIDEO))
      throw std::runtime_error(std::string("sdl init error: ") + SDL_GetError());
    win = SDL_CreateWindow("demo", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (win == nullptr)
      throw std::runtime_error(std::string("could not create sdl window: ") + SDL_GetError());

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(win, &w, &h);

    auto &inst = VulkanInstance::instance();
    if (!SDL_Vulkan_CreateSurface(win, inst, nullptr, &surface))
      throw std::runtime_error(std::string("could not create vk surface: ") + SDL_GetError());

    inst.enableDebug();
    auto dev = inst.createDevice("NVIDIA");

    view = std::make_shared<ShadowView>(dev);
    view->setSurface(surface, w, h);
    view->createPipeline();
  } catch (std::runtime_error &e) {
    printf("%s", e.what());
    return -1;
  }
  view->frame();
  view.reset();
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
